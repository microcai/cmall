/* This file was generated by ODB, object-relational mapping (ORM)
 * compiler for C++.
 */

ALTER TABLE "cmall_order_bought_goods"
  ALTER COLUMN "value_selection" SET NOT NULL;

UPDATE "schema_version"
  SET "migration" = FALSE
  WHERE "name" = '';

