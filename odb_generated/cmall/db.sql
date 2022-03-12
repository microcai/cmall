/* This file was generated by ODB, object-relational mapping (ORM)
 * compiler for C++.
 */

DROP TABLE IF EXISTS "cmall_apply_for_mechant" CASCADE;

DROP TABLE IF EXISTS "cmall_cart" CASCADE;

DROP TABLE IF EXISTS "cmall_order_bought_goods" CASCADE;

DROP TABLE IF EXISTS "cmall_order_recipient" CASCADE;

DROP TABLE IF EXISTS "cmall_order" CASCADE;

DROP TABLE IF EXISTS "cmall_merchant" CASCADE;

DROP TABLE IF EXISTS "administrators" CASCADE;

DROP TABLE IF EXISTS "cmall_user_recipients" CASCADE;

DROP TABLE IF EXISTS "cmall_user_used_phones" CASCADE;

DROP TABLE IF EXISTS "cmall_user" CASCADE;

DROP TABLE IF EXISTS "cmall_config" CASCADE;

CREATE TABLE IF NOT EXISTS "schema_version" (
  "name" TEXT NOT NULL PRIMARY KEY,
  "version" BIGINT NOT NULL,
  "migration" BOOLEAN NOT NULL);

DELETE FROM "schema_version"
  WHERE "name" = '';

CREATE TABLE "cmall_config" (
  "id" BIGSERIAL NOT NULL PRIMARY KEY);

CREATE TABLE "cmall_user" (
  "uid" BIGSERIAL NOT NULL PRIMARY KEY,
  "name" TEXT NOT NULL,
  "active_phone" TEXT NOT NULL,
  "verified" BOOLEAN NOT NULL,
  "state" SMALLINT NOT NULL,
  "desc" TEXT NULL,
  "created_at" TIMESTAMP NULL,
  "updated_at" TIMESTAMP NULL,
  "deleted_at" TIMESTAMP NULL);

CREATE INDEX "cmall_user_name_i"
  ON "cmall_user" ("name");

CREATE INDEX "cmall_user_active_phone_i"
  ON "cmall_user" ("active_phone");

CREATE TABLE "cmall_user_used_phones" (
  "object_id" BIGINT NOT NULL,
  "index" BIGINT NOT NULL,
  "value" TEXT NOT NULL,
  CONSTRAINT "object_id_fk"
    FOREIGN KEY ("object_id")
    REFERENCES "cmall_user" ("uid")
    ON DELETE CASCADE);

CREATE INDEX "cmall_user_used_phones_object_id_i"
  ON "cmall_user_used_phones" ("object_id");

CREATE INDEX "cmall_user_used_phones_index_i"
  ON "cmall_user_used_phones" ("index");

CREATE TABLE "cmall_user_recipients" (
  "object_id" BIGINT NOT NULL,
  "index" BIGINT NOT NULL,
  "value_name" TEXT NOT NULL,
  "value_telephone" TEXT NOT NULL,
  "value_address" TEXT NOT NULL,
  "value_province" TEXT NULL,
  "value_city" TEXT NULL,
  "value_district" TEXT NULL,
  "value_specific_address" TEXT NULL,
  "value_as_default" BOOLEAN NOT NULL DEFAULT FALSE,
  CONSTRAINT "object_id_fk"
    FOREIGN KEY ("object_id")
    REFERENCES "cmall_user" ("uid")
    ON DELETE CASCADE);

CREATE INDEX "cmall_user_recipients_object_id_i"
  ON "cmall_user_recipients" ("object_id");

CREATE INDEX "cmall_user_recipients_index_i"
  ON "cmall_user_recipients" ("index");

CREATE TABLE "administrators" (
  "uid" BIGSERIAL NOT NULL PRIMARY KEY,
  "user" BIGINT NULL,
  CONSTRAINT "user_fk"
    FOREIGN KEY ("user")
    REFERENCES "cmall_user" ("uid")
    INITIALLY DEFERRED);

CREATE TABLE "cmall_merchant" (
  "uid" BIGINT NOT NULL PRIMARY KEY,
  "name" TEXT NOT NULL,
  "verified" BOOLEAN NOT NULL,
  "state" SMALLINT NOT NULL,
  "desc" TEXT NULL,
  "api_token" TEXT NULL,
  "created_at" TIMESTAMP NULL,
  "updated_at" TIMESTAMP NULL,
  "deleted_at" TIMESTAMP NULL,
  "repo_path" TEXT NOT NULL);

CREATE INDEX "cmall_merchant_name_i"
  ON "cmall_merchant" ("name");

CREATE TABLE "cmall_order" (
  "id" BIGSERIAL NOT NULL PRIMARY KEY,
  "oid" TEXT NOT NULL,
  "buyer" BIGINT NOT NULL,
  "seller" BIGINT NOT NULL,
  "price" NUMERIC NOT NULL,
  "pay_amount" numeric NOT NULL DEFAULT '0',
  "stage" SMALLINT NOT NULL,
  "payed_at" TIMESTAMP NULL,
  "close_at" TIMESTAMP NULL,
  "snap_git_version" TEXT NOT NULL DEFAULT '',
  "ext" TEXT NULL,
  "created_at" TIMESTAMP NULL,
  "updated_at" TIMESTAMP NULL,
  "deleted_at" TIMESTAMP NULL);

CREATE UNIQUE INDEX "cmall_order_oid_i"
  ON "cmall_order" ("oid");

CREATE INDEX "cmall_order_buyer_i"
  ON "cmall_order" ("buyer");

CREATE INDEX "cmall_order_seller_i"
  ON "cmall_order" ("seller");

CREATE INDEX "cmall_order_snap_git_version_i"
  ON "cmall_order" ("snap_git_version");

CREATE TABLE "cmall_order_recipient" (
  "object_id" BIGINT NOT NULL,
  "index" BIGINT NOT NULL,
  "value_name" TEXT NOT NULL,
  "value_telephone" TEXT NOT NULL,
  "value_address" TEXT NOT NULL,
  "value_province" TEXT NULL,
  "value_city" TEXT NULL,
  "value_district" TEXT NULL,
  "value_specific_address" TEXT NULL,
  "value_as_default" BOOLEAN NOT NULL DEFAULT FALSE,
  CONSTRAINT "object_id_fk"
    FOREIGN KEY ("object_id")
    REFERENCES "cmall_order" ("id")
    ON DELETE CASCADE);

CREATE INDEX "cmall_order_recipient_object_id_i"
  ON "cmall_order_recipient" ("object_id");

CREATE INDEX "cmall_order_recipient_index_i"
  ON "cmall_order_recipient" ("index");

CREATE TABLE "cmall_order_bought_goods" (
  "object_id" BIGINT NOT NULL,
  "index" BIGINT NOT NULL,
  "value_merchant_id" BIGINT NOT NULL,
  "value_goods_id" TEXT NOT NULL,
  "value_name" TEXT NOT NULL,
  "value_price" NUMERIC NOT NULL,
  "value_description" TEXT NOT NULL,
  "value_good_version_git" TEXT NOT NULL,
  CONSTRAINT "object_id_fk"
    FOREIGN KEY ("object_id")
    REFERENCES "cmall_order" ("id")
    ON DELETE CASCADE);

CREATE INDEX "cmall_order_bought_goods_object_id_i"
  ON "cmall_order_bought_goods" ("object_id");

CREATE INDEX "cmall_order_bought_goods_index_i"
  ON "cmall_order_bought_goods" ("index");

CREATE TABLE "cmall_cart" (
  "id" BIGSERIAL NOT NULL PRIMARY KEY,
  "uid" BIGINT NOT NULL,
  "merchant_id" BIGINT NOT NULL,
  "goods_id" TEXT NOT NULL,
  "count" BIGINT NOT NULL,
  "created_at" TIMESTAMP NULL,
  "updated_at" TIMESTAMP NULL);

CREATE INDEX "cmall_cart_uid_i"
  ON "cmall_cart" ("uid");

CREATE TABLE "cmall_apply_for_mechant" (
  "id" BIGSERIAL NOT NULL PRIMARY KEY,
  "applicant" BIGINT NULL,
  "ext" TEXT NOT NULL,
  "created_at" TIMESTAMP NULL,
  "updated_at" TIMESTAMP NULL,
  "deleted_at" TIMESTAMP NULL,
  CONSTRAINT "applicant_fk"
    FOREIGN KEY ("applicant")
    REFERENCES "cmall_user" ("uid")
    INITIALLY DEFERRED);

CREATE INDEX "cmall_apply_for_mechant_applicant_i"
  ON "cmall_apply_for_mechant" ("applicant");

INSERT INTO "schema_version" (
  "name", "version", "migration")
  SELECT '', 13, FALSE
  WHERE NOT EXISTS (
    SELECT 1 FROM "schema_version" WHERE "name" = '');

