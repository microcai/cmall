/* This file was generated by ODB, object-relational mapping (ORM)
 * compiler for C++.
 */

ALTER TABLE "cmall_merchant"
  DROP COLUMN "api_token";

UPDATE "schema_version"
  SET "migration" = FALSE
  WHERE "name" = '';

