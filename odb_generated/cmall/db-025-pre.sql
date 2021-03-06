/* This file was generated by ODB, object-relational mapping (ORM)
 * compiler for C++.
 */

CREATE TABLE "cmall_user_fav" (
  "id" BIGSERIAL NOT NULL PRIMARY KEY,
  "uid" BIGINT NOT NULL,
  "merchant_id" BIGINT NOT NULL,
  "created_at" TIMESTAMP NULL,
  "updated_at" TIMESTAMP NULL);

CREATE INDEX "cmall_user_fav_uid_i"
  ON "cmall_user_fav" ("uid");

UPDATE "schema_version"
  SET "version" = 25, "migration" = TRUE
  WHERE "name" = '';

