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

/*
 * Take https://github.com/kaigai/ctidscan.git as example
 */

PG_MODULE_MAGIC;

// TODO: extern global variable for capi-regex, consider to remove. 
extern uint32_t PATTERN_ID;
extern uint32_t PACKET_ID;

/*
 * PGCAPIScanState - state object of PGCAPIscan on executor.
 * Job descriptors and relation information is passed with this struct.
 */
typedef struct {
    CustomScanState         css;

    // Relation related variables
    List*                   PGCAPI_quals;
    AttInMetadata*          attinmeta;

    // Capi related variables and job descriptors
    const char*             capi_regex_pattern;
    int                     capi_regex_attr_id;
    CAPIRegexJobDescriptor* job_desc;
} PGCAPIScanState;

/*
 * Static variables
 */
static bool enable_PGCAPIscan;
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

/*
 * CustomScanExecMethods 
 * All customer scan related methods are defined here.
 */
static void BeginPGCAPIScan (CustomScanState* node, EState* estate, int eflags);
static void ReScanPGCAPIScan (CustomScanState* node);
static TupleTableSlot* ExecPGCAPIScan (CustomScanState* node);
static void EndPGCAPIScan (CustomScanState* node);
static void ExplainPGCAPIScan (CustomScanState* node, shm_toc* ancestors, void* es);

/* 
 * static table of custom-scan callbacks 
 *
 */
static CustomPathMethods    PGCAPIscan_path_methods = {
    "PGCAPIscan",               /* CustomName */
    PlanPGCAPIScanPath,         /* PlanCustomPath */
#if PG_VERSION_NUM < 90600
    NULL,                       /* TextOutCustomPath */
#endif
};

static CustomScanMethods    PGCAPIscan_scan_methods = {
    "PGCAPIscan",               /* CustomName */
    CreatePGCAPIScanState,      /* CreateCustomScanState */
#if PG_VERSION_NUM < 90600
    NULL,                       /* TextOutCustomScan */
#endif
};

static CustomExecMethods    PGCAPIscan_exec_methods = {
    "PGCAPIscan",               /* CustomName */
    BeginPGCAPIScan,            /* BeginCustomScan */
    ExecPGCAPIScan,             /* ExecCustomScan */
    EndPGCAPIScan,              /* EndCustomScan */
    ReScanPGCAPIScan,           /* ReScanCustomScan */
    NULL,                       /* MarkPosCustomScan */
    NULL,                       /* RestrPosCustomScan */
#if PG_VERSION_NUM >= 90600
    NULL,                       /* EstimateDSMCustomScan */
    NULL,                       /* InitializeDSMCustomScan */
    NULL,                       /* InitializeWorkerCustomScan */
#endif
    ExplainPGCAPIScan,          /* ExplainCustomScan */
};

#define IsPGCAPIVar(node,rtindex)                                           \
    ((node) != NULL &&                                                  \
     IsA((node), Var) &&                                                \
     ((Var *) (node))->varno == (rtindex) &&                            \
     ((Var *) (node))->varlevelsup == 0)

/*
 * The regular expression matching procedure on CAPI
 */
static void
regex_capi (CustomScanState* node)
{
    PGCAPIScanState*  capiss = (PGCAPIScanState*) node;

    const char* i_pattern = capiss->capi_regex_pattern;
    const AttrNumber i_attr_id = (capiss->capi_regex_attr_id - 1);

    if (NULL == i_pattern) {
        elog (ERROR, "Invalid pattern pointer for CAPI task!");
    }

    if (i_attr_id < 0) {
        elog (ERROR, "Invalid attribute id for CAPI task!");
    }

    if (NULL == capiss->job_desc) {
        elog (ERROR, "Invalid job descriptorfor CAPI task!");
    }

    Relation rel = capiss->css.ss.ss_currentRelation;

    elog (INFO, "regex_capi start");

    PATTERN_ID = 0;
    PACKET_ID = 0;

    struct timespec t_beg, t_end;
    clock_gettime (CLOCK_REALTIME, &t_beg);

    if (!capi_regex_check_relation (rel)) {
        elog (ERROR, "regex_capi cannot use the relation %s\n", RelationGetRelationName (rel));
    } else {
        PERF_MEASURE (capi_regex_job_init       (capiss->job_desc),                 capiss->job_desc->t_init);
        PERF_MEASURE (capi_regex_compile        (capiss->job_desc, i_pattern),      capiss->job_desc->t_regex_patt);
        PERF_MEASURE (capi_regex_pkt_psql       (capiss->job_desc, rel, i_attr_id), capiss->job_desc->t_regex_pkt);
        PERF_MEASURE (capi_regex_scan           (capiss->job_desc),                 capiss->job_desc->t_regex_scan);
        PERF_MEASURE (capi_regex_result_harvest (capiss->job_desc),                 capiss->job_desc->t_regex_harvest);
    }

fail:
    clock_gettime (CLOCK_REALTIME, &t_end);
    print_time_text ("|The total run time|", diff_time (&t_beg, &t_end) / 1000, capiss->job_desc->pkt_size_wo_hw_hdr);
    elog (INFO, "regex_capi done");
}

/*
 * PGCAPIQualFromExpr
 * Get expression (restrictinfo) for later usage.
 */
static List*
PGCAPIQualFromExpr (Node* expr, int varno)
{
    if (is_opclause (expr)) {
        OpExpr* op = (OpExpr*) expr;
        Node*   arg1;
        Node*   arg2;
        Node*   other = NULL;

        arg1 = linitial (op->args);
        arg2 = lsecond (op->args);

        /* only inequality operators are candidate */
        if (op->opno != OID_NAME_REGEXEQ_OP &&
            op->opno != OID_TEXT_REGEXEQ_OP) {
            return NULL;
        }

        if (list_length (op->args) != 2) {
            return NULL;    /* should not happen */
        }

        if (IsPGCAPIVar (arg1, varno)) {
            other = arg2;
        } else if (IsPGCAPIVar (arg2, varno)) {
            other = arg1;
        } else {
            return NULL;
        }

        /* The other argument must be a pseudoconstant */
        if (!is_pseudo_constant_clause (other)) {
            return NULL;
        }

        return list_make1 (copyObject (op));
    } else if (and_clause (expr)) {
        // TODO: AND clause is not supported.
        //List*       rlst = NIL;
        //ListCell*   lc;

        //elog (INFO, "In and_clause.");

        //foreach (lc, ((BoolExpr*) expr)->args) {
        //    List*   temp = PGCAPIQualFromExpr ((Node*) lfirst (lc), varno);

        //    rlst = list_concat (rlst, temp);
        //}

        //return rlst;
        return NIL;
    }

    return NIL;
}

/*
 * PGCAPIEstimateCosts
 *
 * This estimation procedure is FAKE! Need to revisit and get a meaningful estimation equation.
*/
static void
PGCAPIEstimateCosts (PlannerInfo* root,
                     RelOptInfo* baserel,
                     CustomPath* cpath)
{
    // TODO: fake cost
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
     * A knob to control if this hook should be enabled.
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

        cpath = (CustomPath*) palloc0 (sizeof (CustomPath));
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
    PGCAPIScanState*  capiss = (PGCAPIScanState*) palloc0 (sizeof (PGCAPIScanState));

    NodeSetTag (capiss, T_CustomScanState);
    capiss->css.flags = custom_plan->flags;
    capiss->css.methods = &PGCAPIscan_exec_methods;

    // Initialize CAPI job descriptor and related variables
    capiss->capi_regex_pattern = NULL;
    capiss->capi_regex_attr_id = -1;
    capiss->job_desc = palloc0 (sizeof (CAPIRegexJobDescriptor));

    return (Node*)&capiss->css;
}

/*
 * BeginPGCAPIScan - A method of CustomScanState; that initializes
 * the supplied PGCAPIScanState object, at beginning of the executor.
 */
static void
BeginPGCAPIScan (CustomScanState* node, EState* estate, int eflags)
{
    PGCAPIScanState*  capiss = (PGCAPIScanState*) node;
    CustomScan*     cscan = (CustomScan*) node->ss.ps.plan;
    ListCell*       lc;

    TupleDesc tupdesc   = RelationGetDescr (capiss->css.ss.ss_currentRelation);
    capiss->attinmeta = TupleDescGetAttInMetadata (tupdesc);

    // Rewind the restriction expression to get the column id (attr id)
    // to be scanned and the pattern in regex expression.
    foreach (lc, cscan->custom_exprs) {
        OpExpr*         op = (OpExpr*) lfirst (lc);
        Node* arg1 = linitial (op->args);
        Node* arg2 = lsecond (op->args);

        if (nodeTag (arg1) == T_Var) {
            AttrNumber t_attr_id = ((Var*) (arg1))->varattno;
            elog (INFO, "Arg1 attr no: %d", t_attr_id);
            capiss->capi_regex_attr_id = t_attr_id;
        }

        if (nodeTag (arg2) == T_Const) {
            Const* t_const = (Const*) arg2;
            bytea* t_ptr = DatumGetByteaP (t_const->constvalue);
            elog (INFO, "Arg2 Size: %lu", VARSIZE_ANY_EXHDR (t_ptr));
            elog (INFO, "Arg2: %s", VARDATA (t_ptr));
            capiss->capi_regex_pattern = VARDATA (t_ptr);
        }
    }

    // Start the CAPI regular expression procedure.
    regex_capi (node);

    // Need to set this qualification variable to NULL so that
    // in ExecScan phase the result is not filtered out.
    (&node->ss)->ps.qual = NULL;
}

/*
 * ReScanPGCAPIScan
 * TODO: Currently this rescan method is FAKE, need to revisit.
 */
static void
ReScanPGCAPIScan (CustomScanState* node)
{
    PGCAPIScanState*  capiss = (PGCAPIScanState*)node;
    HeapScanDesc    scan = capiss->css.ss.ss_currentScanDesc;
    EState*         estate = node->ss.ps.state;
    Relation        relation = capiss->css.ss.ss_currentRelation;

    /* once close the existing scandesc, if any */
    if (scan) {
        heap_endscan (scan);
        scan = capiss->css.ss.ss_currentScanDesc = NULL;
    }

    scan = heap_beginscan (relation, estate->es_snapshot, 0, NULL);
    capiss->css.ss.ss_currentScanDesc = scan;
}

/*
 * PGCAPIAccessCustomScan
 *
 * TODO: The result is not strictly follow the tuple format in the table.
 * Only the row id is returned.
 */
static TupleTableSlot*
PGCAPIAccessCustomScan (CustomScanState* node)
{
    PGCAPIScanState*  capiss = (PGCAPIScanState*) node;
    HeapScanDesc    scan;
    TupleTableSlot* slot;
    char**       values;

    if (capiss->job_desc->curr_result_id >= capiss->job_desc->num_matched_pkt) {
        return NULL;
    }

    values = (char**) palloc (2 * sizeof (char*));
    values[0] = (char*) palloc (16 * sizeof (char));
    values[1] = (char*) palloc (16 * sizeof (char));

    // TODO: need a real column data to be returned
    sprintf (values[0], "Column data");
    sprintf (values[1], "%d", ((uint32_t*)capiss->job_desc->results)[capiss->job_desc->curr_result_id]);
    (capiss->job_desc->curr_result_id)++;

    HeapTuple tuple = BuildTupleFromCStrings (capiss->attinmeta, values);

    if (!capiss->css.ss.ss_currentScanDesc) {
        ReScanPGCAPIScan (node);
    }

    scan = capiss->css.ss.ss_currentScanDesc;
    Assert (scan != NULL);

    if (!HeapTupleIsValid (tuple)) {
        return NULL;
    }

    slot = capiss->css.ss.ss_ScanTupleSlot;
    ExecStoreTuple (tuple, slot, scan->rs_cbuf, false);

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
    return ExecScan (&node->ss,
                     (ExecScanAccessMtd) PGCAPIAccessCustomScan,
                     (ExecScanRecheckMtd) PGCAPIRecheckCustomScan);
}

/*
 * PGCAPIEndCustomScan
 * Close all resources in this method.
 */
static void
EndPGCAPIScan (CustomScanState* node)
{
    PGCAPIScanState*  capiss = (PGCAPIScanState*)node;

    // TODO: Only 4096 for the output?
    char header_str[4096] = "";
    char out_str[4096] = "";

    if (capiss->css.ss.ss_currentScanDesc) {
        heap_endscan (capiss->css.ss.ss_currentScanDesc);
    }

    PERF_MEASURE (capi_regex_job_cleanup (capiss->job_desc), capiss->job_desc->t_cleanup);
    print_result (capiss->job_desc, header_str, out_str);
    char* result = pstrdup (header_str);
    elog (INFO, "Header: %s", result);
    result = pstrdup (out_str);
    elog (INFO, "Result: %s", result);
fail:
    pfree (capiss->job_desc);
}

/*
 * ExplainPGCAPIScan
 */
static void
ExplainPGCAPIScan (CustomScanState* node, shm_toc* ancestors, void* es)
{
    PGCAPIScanState*  capiss = (PGCAPIScanState*) node;
    CustomScan*     cscan = (CustomScan*) capiss->css.ss.ps.plan;

    /* logic copied from show_qual and show_expression */
    if (cscan->custom_exprs) {
        bool    useprefix = ((ExplainState*) es)->verbose;
        Node*   qual;
        List*   context;
        char*   exprstr;

        /* Convert AND list to explicit AND */
        qual = (Node*) make_ands_explicit (cscan->custom_exprs);

        /* Set up deparsing context */
        context = set_deparse_context_planstate (((ExplainState*) es)->deparse_cxt,
                  (Node*) &node->ss.ps,
                  (List*) ancestors);

        /* Deparse the expression */
        exprstr = deparse_expression (qual, context, useprefix, false);

        /* And add to es->str */
        ExplainPropertyText ("PGCAPI quals", exprstr, (ExplainState*) es);
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

