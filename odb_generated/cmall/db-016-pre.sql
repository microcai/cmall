/* This file was generated by ODB, object-relational mapping (ORM)
 * compiler for C++.
 */

ALTER TABLE "cmall_apply_for_mechant"
  ADD COLUMN "approved" BOOLEAN NOT NULL DEFAULT FALSE;

UPDATE "schema_version"
  SET "version" = 16, "migration" = TRUE
  WHERE "name" = '';
