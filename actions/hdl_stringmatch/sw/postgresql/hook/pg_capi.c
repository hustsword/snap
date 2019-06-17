/*
 * Copyright 2019 International Business Machines
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "pg_capi.h"
#include "pg_capi_internal.h"

PG_MODULE_MAGIC;

extern uint32_t PATTERN_ID;
extern uint32_t PACKET_ID;
int tmp = 0;
int tmp_exec = 0;

/*
 * PGCAPIScanState - state object of PGCAPIscan on executor.
 */
typedef struct {
    CustomScanState css;
    List*           PGCAPI_quals;       /* list of ExprState for inequality ops */
} PGCAPIScanState;

/* static variables */
static bool     enable_PGCAPIscan;
static set_rel_pathlist_hook_type set_rel_pathlist_next = NULL;

/* function declarations */
void    _PG_init (void);

static void SetPGCAPIScanPath (PlannerInfo* root,
                               RelOptInfo* rel,
                               Index rti,
                               RangeTblEntry* rte);
/* CustomPathMethods */
static Plan* PlanPGCAPIScanPath (PlannerInfo* root,
                                 RelOptInfo* rel,
                                 CustomPath* best_path,
                                 List* tlist,
                                 List* clauses,
                                 List* custom_plans);

/* CustomScanMethods */
static Node* CreatePGCAPIScanState (CustomScan* custom_plan);

/* CustomScanExecMethods */
static void BeginPGCAPIScan (CustomScanState* node, EState* estate, int eflags);
static void ReScanPGCAPIScan (CustomScanState* node);
static TupleTableSlot* ExecPGCAPIScan (CustomScanState* node);
static void EndPGCAPIScan (CustomScanState* node);
static void ExplainPGCAPIScan (CustomScanState* node, List* ancestors,
                               ExplainState* es);

/* static table of custom-scan callbacks */
static CustomPathMethods    PGCAPIscan_path_methods = {
    "PGCAPIscan",               /* CustomName */
    PlanPGCAPIScanPath,     /* PlanCustomPath */
#if PG_VERSION_NUM < 90600
    NULL,                   /* TextOutCustomPath */
#endif
};

static CustomScanMethods    PGCAPIscan_scan_methods = {
    "PGCAPIscan",               /* CustomName */
    CreatePGCAPIScanState,  /* CreateCustomScanState */
#if PG_VERSION_NUM < 90600
    NULL,                   /* TextOutCustomScan */
#endif
};

static CustomExecMethods    PGCAPIscan_exec_methods = {
    "PGCAPIscan",               /* CustomName */
    BeginPGCAPIScan,            /* BeginCustomScan */
    ExecPGCAPIScan,         /* ExecCustomScan */
    EndPGCAPIScan,          /* EndCustomScan */
    ReScanPGCAPIScan,           /* ReScanCustomScan */
    NULL,                   /* MarkPosCustomScan */
    NULL,                   /* RestrPosCustomScan */
#if PG_VERSION_NUM >= 90600
    NULL,                   /* EstimateDSMCustomScan */
    NULL,                   /* InitializeDSMCustomScan */
    NULL,                   /* InitializeWorkerCustomScan */
#endif
    ExplainPGCAPIScan,      /* ExplainCustomScan */
};

#define IsPGCAPIVar(node,rtindex)                                           \
    ((node) != NULL &&                                                  \
     IsA((node), Var) &&                                                \
     ((Var *) (node))->varno == (rtindex) &&                            \
     ((Var *) (node))->varattno == SelfItemPointerAttributeNumber &&    \
     ((Var *) (node))->varlevelsup == 0)

static Datum
regex_capi (CustomScanState* node)
{
    PGCAPIScanState*  ctss = (PGCAPIScanState*) node;
    text* relname = "test";

    //const char* i_relName = text_to_cstring (relname);
    const char* i_pattern = "abc.*xyz";
    const int32_t i_attr_id = 0;
    //const int32_t i_attr_id = 1;

    // TODO: Only 4096 for the output?
    char out_str[4096] = "";

    //RangeVar* relrv = makeRangeVarFromNameList (textToQualifiedNameList (relname));
    //Relation rel = relation_openrv (relrv, AccessShareLock);
    Relation rel = ctss->css.ss.ss_currentRelation;

    elog (INFO, "regex_capi start");

    CAPIRegexJobDescriptor* job_desc = palloc0 (sizeof (CAPIRegexJobDescriptor));
    PATTERN_ID = 0;
    PACKET_ID = 0;

    struct timespec t_beg, t_end;
    clock_gettime (CLOCK_REALTIME, &t_beg);

    if (!capi_regex_check_relation (rel)) {
        sprintf (out_str, "regex_capi cannot use the relation %s\n", RelationGetRelationName (rel));
    } else {
        PERF_MEASURE (capi_regex_job_init (job_desc),                 job_desc->t_init);
        PERF_MEASURE (capi_regex_compile (job_desc, i_pattern),       job_desc->t_regex_patt);
        PERF_MEASURE (capi_regex_pkt_psql (job_desc, rel, i_attr_id), job_desc->t_regex_pkt);
        PERF_MEASURE (capi_regex_scan (job_desc),                     job_desc->t_regex_scan);
        PERF_MEASURE (capi_regex_result_harvest (job_desc),           job_desc->t_regex_harvest);
    }

fail:
    PERF_MEASURE (capi_regex_job_cleanup (job_desc), job_desc->t_cleanup);
    print_result (job_desc, out_str);

    clock_gettime (CLOCK_REALTIME, &t_end);
    print_time_text ("|The total run time|", diff_time (&t_beg, &t_end) / 1000, job_desc->pkt_size_wo_hw_hdr);
    pfree (job_desc);
    elog (INFO, "regex_capi done");

    char* result = pstrdup (out_str);
    elog (INFO, "Result: %s", result);
    PG_RETURN_CSTRING (result);
}

/*
 * PGCAPIQualFromExpr
 *
 * It checks whether the given restriction clauses enables to determine
 * the zone to be scanned, or not. If one or more restriction clauses are
 * available, it returns a list of them, or NIL elsewhere.
 * The caller can consider all the conditions are chained with AND-
 * boolean operator, so all the operator works for narrowing down the
 * scope of custom tid scan.
 */
static List*
PGCAPIQualFromExpr (Node* expr, int varno)
{
    if (is_opclause (expr)) {
        OpExpr* op = (OpExpr*) expr;
        Node*   arg1;
        Node*   arg2;
        Node*   other = NULL;

        elog (INFO, "In opclause.");
        elog (INFO, "Opno: %d.", op->opno);

        arg1 = linitial (op->args);
        arg2 = lsecond (op->args);

        elog (INFO, "Arg1 nodeTag: %d", nodeTag (arg1));
        elog (INFO, "Arg2 nodeTag: %d", nodeTag (arg2));

        if (nodeTag (arg2) == T_Const) {
            Const* t_const = (Const*) arg2;
            bytea* t_ptr = DatumGetByteaP (t_const->constvalue);
            elog (INFO, "Arg2 Size: %d", VARSIZE_ANY_EXHDR (t_ptr));
            elog (INFO, "Arg2: %s", VARDATA (t_ptr));
        }

        /* only inequality operators are candidate */
        if (op->opno != OID_NAME_REGEXEQ_OP &&
            op->opno != OID_TEXT_REGEXEQ_OP) {
            return NULL;
        }

        if (list_length (op->args) != 2) {
            return false;    /* should not happen */
        }

        //arg1 = linitial (op->args);
        //arg2 = lsecond (op->args);

        //if (IsPGCAPIVar (arg1, varno)) {
        //    other = arg2;
        //} else if (IsPGCAPIVar (arg2, varno)) {
        //    other = arg1;
        //} else {
        //    return NULL;
        //}

        //if (exprType (other) != TIDOID) {
        //    return NULL;    /* should not happen */
        //}

        ///* The other argument must be a pseudoconstant */
        //if (!is_pseudo_constant_clause (other)) {
        //    return NULL;
        //}

        return list_make1 (copyObject (op));
    } else if (and_clause (expr)) {
        //List*       rlst = NIL;
        //ListCell*   lc;

        //elog (INFO, "In and_clause.");

        //foreach (lc, ((BoolExpr*) expr)->args) {
        //    List*   temp = PGCAPIQualFromExpr ((Node*) lfirst (lc), varno);

        //    rlst = list_concat (rlst, temp);
        //}
        return NULL;

        //return rlst;
    }

    return NIL;
}

/*
 * PGCAPIEstimateCosts
 *
 * It estimates cost to scan the target relation according to the given
 * restriction clauses. Its logic to scan relations are almost same as
 * SeqScan doing, because it uses regular heap_getnext(), except for
 * the number of tuples to be scanned if restriction clauses work well.
*/
static void
PGCAPIEstimateCosts (PlannerInfo* root,
                     RelOptInfo* baserel,
                     CustomPath* cpath)
{
    Path*       path = &cpath->path;
    List*       PGCAPI_quals = cpath->custom_private;
    ListCell*   lc;
    double      ntuples;
    ItemPointerData ip_min;
    ItemPointerData ip_max;
    bool        has_min_val = false;
    bool        has_max_val = false;
    BlockNumber num_pages;
    Cost        startup_cost = 0;
    Cost        run_cost = 0;
    Cost        cpu_per_tuple;
    QualCost    qpqual_cost;
    QualCost    PGCAPI_qual_cost;
    double      spc_random_page_cost;

    /* Should only be applied to base relations */
    Assert (baserel->relid > 0);
    Assert (baserel->rtekind == RTE_RELATION);

    /* Mark the path with the correct row estimate */
    if (path->param_info) {
        path->rows = path->param_info->ppi_rows;
    } else {
        path->rows = baserel->rows;
    }

    /* Estimate how many tuples we may retrieve */
    ItemPointerSet (&ip_min, 0, 0);
    ItemPointerSet (&ip_max, MaxBlockNumber, MaxOffsetNumber);

    //foreach (lc, PGCAPI_quals) {
    //    OpExpr*     op = lfirst (lc);
    //    Oid         opno;
    //    Node*       other;

    //    //Assert (is_opclause (op));

    //    //if (IsPGCAPIVar (linitial (op->args), baserel->relid)) {
    //    //    opno = op->opno;
    //    //    other = lsecond (op->args);
    //    //} else if (IsPGCAPIVar (lsecond (op->args), baserel->relid)) {
    //    //    /* To simplifies, we assume as if Var node is 1st argument */
    //    //    opno = get_commutator (op->opno);
    //    //    other = linitial (op->args);
    //    //} else {
    //    //    elog (ERROR, "could not identify PGCAPI variable");
    //    //}

    //    //if (IsA (other, Const)) {
    //    //    ItemPointer ip = (ItemPointer) (((Const*) other)->constvalue);

    //    //    /*
    //    //     * Just an rough estimation, we don't distinct inequality and
    //    //     * inequality-or-equal operator from scan-size estimation
    //    //     * perspective.
    //    //     */
    //    //    switch (opno) {
    //    //    case TIDLessOperator:
    //    //    case TIDLessEqualOperator:
    //    //        if (ItemPointerCompare (ip, &ip_max) < 0) {
    //    //            ItemPointerCopy (ip, &ip_max);
    //    //        }

    //    //        has_max_val = true;
    //    //        break;

    //    //    case TIDGreaterOperator:
    //    //    case TIDGreaterEqualOperator:
    //    //        if (ItemPointerCompare (ip, &ip_min) > 0) {
    //    //            ItemPointerCopy (ip, &ip_min);
    //    //        }

    //    //        has_min_val = true;
    //    //        break;

    //    //    default:
    //    //        elog (ERROR, "unexpected operator code: %u", op->opno);
    //    //        break;
    //    //    }
    //    //}
    //}

    /* estimated number of tuples in this relation */
    ntuples = baserel->pages * baserel->tuples;

    if (has_min_val && has_max_val) {
        /* case of both side being bounded */
        BlockNumber bnum_max = BlockIdGetBlockNumber (&ip_max.ip_blkid);
        BlockNumber bnum_min = BlockIdGetBlockNumber (&ip_min.ip_blkid);

        bnum_max = Min (bnum_max, baserel->pages);
        bnum_min = Max (bnum_min, 0);
        num_pages = Min (bnum_max - bnum_min + 1, 1);
    } else if (has_min_val) {
        /* case of only lower side being bounded */
        BlockNumber bnum_max = baserel->pages;
        BlockNumber bnum_min = BlockIdGetBlockNumber (&ip_min.ip_blkid);

        bnum_min = Max (bnum_min, 0);
        num_pages = Min (bnum_max - bnum_min + 1, 1);
    } else if (has_max_val) {
        /* case of only upper side being bounded */
        BlockNumber bnum_max = BlockIdGetBlockNumber (&ip_max.ip_blkid);
        BlockNumber bnum_min = 0;

        bnum_max = Min (bnum_max, baserel->pages);
        num_pages = Min (bnum_max - bnum_min + 1, 1);
    } else {
        /*
         * Just a rough estimation. We assume half of records shall be
         * read using this restriction clause, but indeterministic until
         * executor run it actually.
         */
        num_pages = Max ((baserel->pages + 1) / 2, 1);
    }

    ntuples *= ((double) num_pages) / ((double) baserel->pages);

    /*
     * The TID qual expressions will be computed once, any other baserestrict
     * quals once per retrieved tuple.
     */
    cost_qual_eval (&PGCAPI_qual_cost, PGCAPI_quals, root);

    /* fetch estimated page cost for tablespace containing table */
    get_tablespace_page_costs (baserel->reltablespace,
                               &spc_random_page_cost,
                               NULL);

    /* disk costs --- assume each tuple on a different page */
    run_cost += spc_random_page_cost * ntuples;

    /*
     * Add scanning CPU costs
     * (logic copied from get_restriction_qual_cost)
     */
    if (path->param_info) {
        /* Include costs of pushed-down clauses */
        cost_qual_eval (&qpqual_cost, path->param_info->ppi_clauses, root);

        qpqual_cost.startup += baserel->baserestrictcost.startup;
        qpqual_cost.per_tuple += baserel->baserestrictcost.per_tuple;
    } else {
        qpqual_cost = baserel->baserestrictcost;
    }

    /*
     * We don't decrease cost for the inequality operators, because
     * it is subset of qpquals and still in.
     */
    startup_cost += qpqual_cost.startup + PGCAPI_qual_cost.per_tuple;
    cpu_per_tuple = cpu_tuple_cost + qpqual_cost.per_tuple -
                    PGCAPI_qual_cost.per_tuple;
    run_cost = cpu_per_tuple * ntuples;

    // TODO: fake cost
    startup_cost = 0.0;
    run_cost = 0.1;

    //path->startup_cost = startup_cost;
    //path->total_cost = startup_cost + run_cost;
}

/*
 * SetPGCAPIScanPath - entrypoint of the series of custom-scan execution.
 * It adds CustomPath if referenced relation has inequality expressions on
 * the PGCAPI system column.
 */
static void
SetPGCAPIScanPath (PlannerInfo* root, RelOptInfo* baserel,
                   Index rtindex, RangeTblEntry* rte)
{
    char            relkind;
    ListCell*       lc;
    List*           PGCAPI_quals = NIL;

    /* only plain relations are supported */
    if (rte->rtekind != RTE_RELATION) {
        return;
    }

    relkind = get_rel_relkind (rte->relid);

    if (relkind != RELKIND_RELATION &&
        relkind != RELKIND_MATVIEW &&
        relkind != RELKIND_TOASTVALUE) {
        return;
    }

    /*
     * NOTE: Unlike built-in execution path, always we can have core path
     * even though PGCAPI scan is not available. So, simply, we don't add
     * any paths, instead of adding disable_cost.
     */
    if (!enable_PGCAPIscan) {
        return;
    }

    /* walk on the restrict info */
    foreach (lc, baserel->baserestrictinfo) {
        RestrictInfo* rinfo = (RestrictInfo*) lfirst (lc);
        List*         temp;

        if (!IsA (rinfo, RestrictInfo)) {
            continue;    /* probably should never happen */
        }

        temp = PGCAPIQualFromExpr ((Node*) rinfo->clause, baserel->relid);
        PGCAPI_quals = list_concat (PGCAPI_quals, temp);
    }

    /*
     * OK, it is case when a part of restriction clause makes sense to
     * reduce number of tuples, so we will add a custom scan path being
     * provided by this module.
     */
    if (PGCAPI_quals != NIL) {

        CustomPath* cpath;
        Relids      required_outer;

        /*
         * We don't support pushing join clauses into the quals of a PGCAPIscan,
         * but it could still have required parameterization due to LATERAL
         * refs in its tlist.
         */
        required_outer = baserel->lateral_relids;

        cpath = palloc0 (sizeof (CustomPath));
        cpath->path.type = T_CustomPath;
        cpath->path.pathtype = T_CustomScan;
        cpath->path.parent = baserel;
#if PG_VERSION_NUM >= 90600
        cpath->path.pathtarget = baserel->reltarget;
#endif
        cpath->path.param_info
            = get_baserel_parampathinfo (root, baserel, required_outer);
        cpath->flags = CUSTOMPATH_SUPPORT_BACKWARD_SCAN;
        cpath->custom_private = PGCAPI_quals;
        cpath->methods = &PGCAPIscan_path_methods;

        PGCAPIEstimateCosts (root, baserel, cpath);

        add_path (baserel, &cpath->path);
    }
}

/*
 * PlanPGCAPIScanPlan - A method of CustomPath; that populate a custom
 * object being delivered from CustomScan type, according to the supplied
 * CustomPath object.
 */
static Plan*
PlanPGCAPIScanPath (PlannerInfo* root,
                    RelOptInfo* rel,
                    CustomPath* best_path,
                    List* tlist,
                    List* clauses,
                    List* custom_plans)
{
    List*           PGCAPI_quals = best_path->custom_private;
    CustomScan*     cscan = makeNode (CustomScan);

    cscan->flags = best_path->flags;
    cscan->methods = &PGCAPIscan_scan_methods;

    /* set scanrelid */
    cscan->scan.scanrelid = rel->relid;
    /* set targetlist as is  */
    cscan->scan.plan.targetlist = tlist;
    /* reduce RestrictInfo list to bare expressions */
    cscan->scan.plan.qual = extract_actual_clauses (clauses, false);
    /* set PGCAPI related quals */
    cscan->custom_exprs = PGCAPI_quals;

    return &cscan->scan.plan;
}

/*
 * CreatePGCAPIScanState - A method of CustomScan; that populate a custom
 * object being delivered from CustomScanState type, according to the
 * supplied CustomPath object.
 */
static Node*
CreatePGCAPIScanState (CustomScan* custom_plan)
{
    PGCAPIScanState*  ctss = palloc0 (sizeof (PGCAPIScanState));

    NodeSetTag (ctss, T_CustomScanState);
    ctss->css.flags = custom_plan->flags;
    ctss->css.methods = &PGCAPIscan_exec_methods;

    return (Node*)&ctss->css;
}

/*
 * BeginPGCAPIScan - A method of CustomScanState; that initializes
 * the supplied PGCAPIScanState object, at beginning of the executor.
 */
static void
BeginPGCAPIScan (CustomScanState* node, EState* estate, int eflags)
{
    PGCAPIScanState*  ctss = (PGCAPIScanState*) node;
    CustomScan*     cscan = (CustomScan*) node->ss.ps.plan;
    //Relation        relation = ctss->css.ss.ss_currentRelation;

    regex_capi (node);
    (&node->ss)->ps.qual = NULL;
    //ctss->css.ss.ss_currentScanDesc = heap_beginscan (relation, estate->es_snapshot, 0, NULL);
    /*
     * In case of custom-scan provider that offers an alternative way
     * to scan a particular relation, most of the needed initialization,
     * like relation open or assignment of scan tuple-slot or projection
     * info, shall be done by the core implementation. So, all we need
     * to have is initialization of own local properties.
     */
#if PG_VERSION_NUM < 100000
    //ctss->PGCAPI_quals = (List*)
    //                     ExecInitExpr ((Expr*)cscan->custom_exprs, &node->ss.ps);
#else
    //ctss->PGCAPI_quals = ExecInitQual (cscan->custom_exprs, &node->ss.ps);
#endif
}

/*
 * ReScanPGCAPIScan - A method of CustomScanState; that rewind the current
 * seek position.
 */
static void
ReScanPGCAPIScan (CustomScanState* node)
{
    PGCAPIScanState*  ctss = (PGCAPIScanState*)node;
    HeapScanDesc    scan = ctss->css.ss.ss_currentScanDesc;
    EState*         estate = node->ss.ps.state;
    ScanDirection   direction = estate->es_direction;
    Relation        relation = ctss->css.ss.ss_currentRelation;
    ExprContext*    econtext = ctss->css.ss.ps.ps_ExprContext;
    ScanKeyData     keys[2];
    bool            has_ubound = false;
    bool            has_lbound = false;
    ItemPointerData ip_max;
    ItemPointerData ip_min;
    ListCell*       lc;

    /* once close the existing scandesc, if any */
    if (scan) {
        heap_endscan (scan);
        scan = ctss->css.ss.ss_currentScanDesc = NULL;
    }

    ///* walks on the inequality operators */
    //foreach (lc, ctss->PGCAPI_quals) {
    //    //FuncExprState*  fexstate = (FuncExprState*) lfirst (lc);
    //    OpExpr*         op = (OpExpr*) lfirst (lc);
    //    Node*           arg1 = linitial (op->args);
    //    Node*           arg2 = lsecond (op->args);
    //    Index           scanrelid;
    //    Oid             opno;
    //    ExprState*      exstate;
    //    ItemPointer     itemptr;
    //    bool            isnull;

    //    scanrelid = ((Scan*)ctss->css.ss.ps.plan)->scanrelid;

    //    if (IsPGCAPIVar (arg1, scanrelid)) {
    //        exstate = (ExprState*) lsecond (op->args);
    //        opno = op->opno;
    //    } else if (IsPGCAPIVar (arg2, scanrelid)) {
    //        exstate = (ExprState*) linitial (op->args);
    //        opno = get_commutator (op->opno);
    //    } else {
    //        elog (ERROR, "could not identify PGCAPI variable");
    //    }

    //    itemptr = (ItemPointer)
    //              DatumGetPointer (ExecEvalExprSwitchContext (exstate,
    //                               econtext,
    //                               &isnull));

    //    if (isnull) {
    //        /*
    //         * Whole of the restriction clauses chained with AND- boolean
    //         * operators because false, if one of the clauses has NULL result.
    //         * So, we can immediately break the evaluation to inform caller
    //         * it does not make sense to scan any more.
    //         * In this case, scandesc is kept to NULL.
    //         */
    //        return;
    //    }

    //    switch (opno) {
    //    case TIDLessOperator:
    //        if (!has_ubound ||
    //            ItemPointerCompare (itemptr, &ip_max) <= 0) {
    //            ScanKeyInit (&keys[0],
    //                         SelfItemPointerAttributeNumber,
    //                         BTLessStrategyNumber,
    //                         F_TIDLT,
    //                         PointerGetDatum (itemptr));
    //            ItemPointerCopy (itemptr, &ip_max);
    //            has_ubound = true;
    //        }

    //        break;

    //    case TIDLessEqualOperator:
    //        if (!has_ubound ||
    //            ItemPointerCompare (itemptr, &ip_max) < 0) {
    //            ScanKeyInit (&keys[0],
    //                         SelfItemPointerAttributeNumber,
    //                         BTLessEqualStrategyNumber,
    //                         F_TIDLE,
    //                         PointerGetDatum (itemptr));
    //            ItemPointerCopy (itemptr, &ip_max);
    //            has_ubound = true;
    //        }

    //        break;

    //    case TIDGreaterOperator:
    //        if (!has_lbound ||
    //            ItemPointerCompare (itemptr, &ip_min) >= 0) {
    //            ScanKeyInit (&keys[1],
    //                         SelfItemPointerAttributeNumber,
    //                         BTGreaterStrategyNumber,
    //                         F_TIDGT,
    //                         PointerGetDatum (itemptr));
    //            ItemPointerCopy (itemptr, &ip_min);
    //            has_lbound = true;
    //        }

    //        break;

    //    case TIDGreaterEqualOperator:
    //        if (!has_lbound ||
    //            ItemPointerCompare (itemptr, &ip_min) > 0) {
    //            ScanKeyInit (&keys[1],
    //                         SelfItemPointerAttributeNumber,
    //                         BTGreaterEqualStrategyNumber,
    //                         F_TIDGE,
    //                         PointerGetDatum (itemptr));
    //            ItemPointerCopy (itemptr, &ip_min);
    //            has_lbound = true;
    //        }

    //        break;

    //    default:
    //        elog (ERROR, "unsupported operator");
    //        break;
    //    }
    //}

    ///* begin heapscan with the key above */
    //if (has_ubound && has_lbound) {
    //    scan = heap_beginscan (relation, estate->es_snapshot, 2, &keys[0]);
    //} else if (has_ubound) {
    //    scan = heap_beginscan (relation, estate->es_snapshot, 1, &keys[0]);
    //} else if (has_lbound) {
    //    scan = heap_beginscan (relation, estate->es_snapshot, 1, &keys[1]);
    //} else {
    scan = heap_beginscan (relation, estate->es_snapshot, 0, NULL);
    //}

    ///* Seek the starting position, if possible */
    //if (direction == ForwardScanDirection && has_lbound) {
    //    BlockNumber blknum = Min (BlockIdGetBlockNumber (&ip_min.ip_blkid),
    //                              scan->rs_nblocks - 1);
    //    scan->rs_startblock = blknum;
    //} else if (direction == BackwardScanDirection && has_ubound) {
    //    BlockNumber blknum = Min (BlockIdGetBlockNumber (&ip_max.ip_blkid),
    //                              scan->rs_nblocks - 1);
    //    scan->rs_startblock = blknum;
    //}

    ctss->css.ss.ss_currentScanDesc = scan;

}

/*
 * PGCAPIAccessCustomScan
 *
 * Access method of ExecPGCAPIScan below. It fetches a tuple from the underlying
 * heap scan that was  started from the point according to the tid clauses.
 */
static TupleTableSlot*
PGCAPIAccessCustomScan (CustomScanState* node)
{
    PGCAPIScanState*  ctss = (PGCAPIScanState*) node;
    HeapScanDesc    scan;
    TupleTableSlot* slot;
    char**       values;
    values = (char**) palloc (2 * sizeof (char*));
    values[0] = (char*) palloc (16 * sizeof (char));
    values[1] = (char*) palloc (16 * sizeof (char));
    sprintf (values[0], "Test 0");
    sprintf (values[1], "0");
    //sprintf(values[0], "0");
    //sprintf(values[1], "Test 0");

    if (tmp > 10) {
        return NULL;
    }

    EState*         estate = node->ss.ps.state;
    ScanDirection   direction = estate->es_direction;
    HeapTuple       tuple;
    TupleDesc       tupdesc  = RelationGetDescr (ctss->css.ss.ss_currentRelation);
    AttInMetadata*  attinmeta;
    attinmeta =     TupleDescGetAttInMetadata (tupdesc);
    tuple = BuildTupleFromCStrings (attinmeta, values);
    ////int attr_len = 0;

    if (!ctss->css.ss.ss_currentScanDesc) {
        ReScanPGCAPIScan (node);
    }

    scan = ctss->css.ss.ss_currentScanDesc;
    //Assert (scan != NULL);

    ///*
    // * get the next tuple from the table
    // */
    //tuple = heap_getnext (scan, direction);

    if (!HeapTupleIsValid (tuple)) {
        return NULL;
    }

    tmp++;
    elog (INFO, "Tuple %d", tmp);
    ////bytea* attr_ptr = DatumGetByteaP (get_attr (tuple->t_data, tupdesc, tuple->t_len, 1, &attr_len));

    ////attr_len = VARSIZE (attr_ptr) - VARHDRSZ;
    ////elog (DEBUG1, "attr len %d : %s", attr_len, VARDATA (attr_ptr));

    slot = ctss->css.ss.ss_ScanTupleSlot;
    ExecStoreTuple (tuple, slot, scan->rs_cbuf, false);
    //ExecStoreTuple (tuple, slot, InvalidBuffer, false);

    return slot;
}

static bool
PGCAPIRecheckCustomScan (CustomScanState* node, TupleTableSlot* slot)
{
    return true;
}

/*
 * ExecPGCAPIScan - A method of CustomScanState; that fetches a tuple
 * from the relation, if exist anymore.
 */
static TupleTableSlot*
ExecPGCAPIScan (CustomScanState* node)
{
    TupleTableSlot* ret = ExecScan (&node->ss,
                                    (ExecScanAccessMtd) PGCAPIAccessCustomScan,
                                    (ExecScanRecheckMtd) PGCAPIRecheckCustomScan);
    return ret;
}

/*
 * PGCAPIEndCustomScan - A method of CustomScanState; that closes heap and
 * scan descriptor, and release other related resources.
 */
static void
EndPGCAPIScan (CustomScanState* node)
{
    PGCAPIScanState*  ctss = (PGCAPIScanState*)node;

    if (ctss->css.ss.ss_currentScanDesc) {
        heap_endscan (ctss->css.ss.ss_currentScanDesc);
    }
}

/*
 * ExplainPGCAPIScan - A method of CustomScanState; that shows extra info
 * on EXPLAIN command.
 */
static void
ExplainPGCAPIScan (CustomScanState* node, List* ancestors, ExplainState* es)
{
    PGCAPIScanState*  ctss = (PGCAPIScanState*) node;
    CustomScan*     cscan = (CustomScan*) ctss->css.ss.ps.plan;

    /* logic copied from show_qual and show_expression */
    if (cscan->custom_exprs) {
        bool    useprefix = es->verbose;
        Node*   qual;
        List*   context;
        char*   exprstr;

        /* Convert AND list to explicit AND */
        qual = (Node*) make_ands_explicit (cscan->custom_exprs);

        /* Set up deparsing context */
        context = set_deparse_context_planstate (es->deparse_cxt,
                  (Node*) &node->ss.ps,
                  ancestors);

        /* Deparse the expression */
        exprstr = deparse_expression (qual, context, useprefix, false);

        /* And add to es->str */
        ExplainPropertyText ("PGCAPI quals", exprstr, es);
    }
}

/*
 * Entrypoint of this extension
 */
void
_PG_init (void)
{
    DefineCustomBoolVariable ("enable_PGCAPIscan",
                              "Enables the planner's use of PGCAPI-scan plans.",
                              NULL,
                              &enable_PGCAPIscan,
                              true,
                              PGC_USERSET,
                              GUC_NOT_IN_SAMPLE,
                              NULL, NULL, NULL);

    /* registration of the hook to add alternative path */
    set_rel_pathlist_next = set_rel_pathlist_hook;
    set_rel_pathlist_hook = SetPGCAPIScanPath;
}
