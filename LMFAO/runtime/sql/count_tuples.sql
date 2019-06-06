create or replace function 
count_rows(schema text, tablename text) returns integer
as
$body$
declare
  result integer;
  query varchar;
begin
          query := 'SELECT count(1) FROM ' || schema || '.' || tablename;
          execute query into result;
          return result;
end;
$body$
language plpgsql;


select 
  table_schema,
  table_name, 
  count_rows(table_schema, table_name)
from information_schema.tables
where 
  table_schema not in ('pg_catalog', 'information_schema') 
  and table_type='BASE TABLE'
order by 3 desc
