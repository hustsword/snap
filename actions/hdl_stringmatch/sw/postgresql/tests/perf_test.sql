SET client_min_messages = warning;
\set ECHO none
\set ECHO all
RESET client_min_messages;

-- Create the perf test data tables if not exist
CREATE OR REPLACE FUNCTION create_table()
RETURNS void AS
$_$
DECLARE
r           character varying;    
_cmd        text;
split_data  text[];
BEGIN
    SELECT INTO split_data regexp_split_to_array('4K,8K,16K,32K,64K,128K,256K,512K',',');
    FOREACH r IN array split_data LOOP
        _cmd := 
            format(
                'CREATE TABLE IF NOT EXISTS perf_test_%1$s(pkt text, id SERIAL);',
                r
            );
        EXECUTE _cmd;
    END LOOP;
END;
$_$ LANGUAGE plpgsql;

-- Copy data to perf test data tables if not exist
CREATE OR REPLACE FUNCTION count_line(tbl text) RETURNS INTEGER
AS $$
DECLARE total INTEGER;
BEGIN
    EXECUTE format('select count(*) from %s limit 1', tbl) into total;
    RETURN total::integer;
END;
$$  LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION copy_if_not_exists ()
RETURNS void AS
$_$
DECLARE
r           character varying;    
_cmd        text;
split_data  text[];
BEGIN
    SELECT INTO split_data regexp_split_to_array('4K,8K,16K,32K,64K,128K,256K,512K',',');
    FOREACH r IN array split_data LOOP
        IF count_line(format('perf_test_%s', r)) = 0 THEN
            _cmd := 
                format(
                    'COPY perf_test_%1$s (pkt) FROM ''/home/pengfei/capi/snap/actions/hdl_stringmatch/tests/packets/perf_test/packet.1024.%1$s.txt'' DELIMITER '' ''; ',
                    r
                );
            EXECUTE _cmd;
        END IF;
    END LOOP;
END;
$_$ LANGUAGE plpgsql;

CREATE table if not exists perf_data (test_name text, regex_capi text, regex_capi_win text, regexp_matches text, where_clause text, ts timestamp);
-- Create a table to store perf data
CREATE OR REPLACE FUNCTION regex_capi_perf_test()
RETURNS void AS
$func$
DECLARE
v_table text;
rc_regexp text;
rc_where text;
rc_regex_capi text;
rc_regex_capi_win text;
BEGIN
    FOR v_table IN
        SELECT table_name  
        FROM   information_schema.tables 
        WHERE  table_catalog = 'pengfei' 
        AND    table_schema = 'public'
        AND    table_name LIKE 'perf_test_%'
        LOOP
            execute format('EXPLAIN (ANALYZE, FORMAT JSON) SELECT pkt, (regexp_matches(pkt, ''abc.*xyz''))[0] from %I'
                , v_table) into rc_regexp;
            execute format('EXPLAIN (ANALYZE, FORMAT JSON) SELECT * FROM %I WHERE pkt ~ ''abc.*xyz'''
                , v_table) into rc_where;
            --execute format('EXPLAIN (ANALYZE, format JSON) SELECT psql_regex_capi(''%I'', ''abc.*xyz'', 0)'
            --    , v_table) into rc_regex_capi;
            --execute format('EXPLAIN (ANALYZE, format JSON) psql_regex_capi_win(pkt, ''abc.*xyz'') over(), pkt, id from perf_test_%I'
            --    , v_table) into rc_regex_capi_win;
            insert into perf_data (test_name, regex_capi, regex_capi_win, regexp_matches, where_clause, ts) values (
                format('%s', v_table),
                --rc_regex_capi::jsonb-> 0 -> 'Execution Time',
                --rc_regex_capi_win::jsonb-> 0 -> 'Execution Time',
                '0',
                '0',
                rc_regexp::jsonb-> 0 -> 'Execution Time',
                rc_where::jsonb-> 0 -> 'Execution Time',
                current_timestamp
            ); 
        END LOOP;
    END;
$func$  LANGUAGE plpgsql;

-- Perform the test
SELECT create_table();
SELECT copy_if_not_exists();
SELECT regex_capi_perf_test();
SELECT * from perf_data;

\copy perf_data TO '/home/pengfei/capi/snap/actions/hdl_stringmatch/sw/postgresql/perf_data.csv' DELIMITER ',' CSV HEADER;
