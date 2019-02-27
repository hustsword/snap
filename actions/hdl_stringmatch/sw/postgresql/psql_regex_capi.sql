CREATE FUNCTION psql_regex_capi(text, text) RETURNS int
AS '$libdir/psql_regex_capi', 'psql_regex_capi'
LANGUAGE C IMMUTABLE STRICT WINDOW;
