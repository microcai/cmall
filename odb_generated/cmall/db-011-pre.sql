/* This file was generated by ODB, object-relational mapping (ORM)
 * compiler for C++.
 */

ALTER TABLE "cmall_merchant"
  ADD COLUMN "api_token" TEXT NULL;

UPDATE "schema_version"
  SET "version" = 11, "migration" = TRUE
  WHERE "name" = '';

