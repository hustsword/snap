#!/bin/bash

install_path=`pg_config  --pkglibdir`

echo "Copying psql_regex_capi.so to $install_path"

cp psql_regex_capi.so $install_path
