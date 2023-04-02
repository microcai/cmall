if(WIN32)
ENABLE_LANGUAGE(ASM_MASM)
else()
ENABLE_LANGUAGE(ASM)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/include/ ../include)

include_directories(asn1)
include_directories(evp)
include_directories(modes)

if (WIN32)
	set(ASYNC_ARCH async/arch/async_win.c async/arch/async_null.c)
elseif (APPLE)
	set(ASYNC_ARCH async/arch/async_null.c)
else()
	set(ASYNC_ARCH async/arch/async_posix.c async/arch/async_null.c)
endif()

file(GLOB SRCS
	aes/*.c

	aria/*.c
	asn1/*.c

	async/*

	bf/bf_skey.c bf/bf_ecb.c bf/bf_cfb64.c bf/bf_ofb64.c bf/bf_enc.c

	bio/*.c
	blake2/*.c
	bn/*.c
	buffer/*.c
	camellia/*.c
	cast/*.c
	chacha/*.c
	cmac/*.c
	cmp/*.c
	cms/*.c
	conf/*.c
	crmf/*.c
	ct/*.c

	dh/*.c

	des/*.c

	dsa/*.c
	dso/*.c

	ec/*.c
	ec/curve448/*.c
	ec/curve448/arch_32/*.c
	ec/curve448/arch_64/*.c

	ecdsa/*.c
	ecdh/*.c
	encode_decode/*.c
	err/*.c
	ess/*.c
	evp/*.c

	ffc/*.c

	hmac/*.c
	http/*.c

	idea/*.c
	kdf/*.c
	lhash/*.c
	md5/*.c

	modes/*.c

	objects/*.c
	ocsp/*.c
	pem/*.c
	pqueue/*.c

	pkcs7/pk7_asn1.c pkcs7/pk7_lib.c pkcs7/pkcs7err.c pkcs7/pk7_doit.c pkcs7/pk7_smime.c pkcs7/pk7_attr.c pkcs7/pk7_mime.c pkcs7/bio_pk7.c

	pkcs12/*.c

	poly1305/poly1305_ameth.c poly1305/poly1305_pmeth.c poly1305/poly1305.c

	property/*.c

	rand/*.c
	rc2/*.c
	rc4/*.c
	rc5/*.c
	rsa/*.c
	ripemd/*.c

	seed/*.c
	sha/*.c
	siphash/siphash_ameth.c  siphash/siphash.c  siphash/siphash_pmeth.c
	sm2/*.c
	sm3/*.c
	sm4/*.c
	srp/*.c
	stack/*.c
	store/*.c
	ts/*.c
	txt_db/*.c
	ui/*.c
	x509/*.c

	x509v3/v3_admis.c x509v3/v3_bcons.c  x509v3/v3_bitst.c  x509v3/v3_conf.c  x509v3/v3_extku.c  x509v3/v3_ia5.c  x509v3/v3_lib.c  x509v3/v3_prn.c  x509v3/v3_utl.c  x509v3/v3err.c  x509v3/v3_genn.c  x509v3/v3_alt.c  x509v3/v3_skey.c  x509v3/v3_akey.c  x509v3/v3_pku.c  x509v3/v3_int.c  x509v3/v3_enum.c  x509v3/v3_sxnet.c  x509v3/v3_cpols.c  x509v3/v3_crld.c  x509v3/v3_purp.c  x509v3/v3_info.c  x509v3/v3_akeya.c  x509v3/v3_pmaps.c  x509v3/v3_pcons.c  x509v3/v3_ncons.c  x509v3/v3_pcia.c  x509v3/v3_pci.c  x509v3/pcy_cache.c  x509v3/pcy_node.c  x509v3/pcy_data.c  x509v3/pcy_map.c  x509v3/pcy_tree.c  x509v3/pcy_lib.c  x509v3/v3_asid.c  x509v3/v3_addr.c  x509v3/v3_tlsf.c

	bsearch.c

	ctype.c cpuid.c
	asn1_dsa.c

	cversion.c  ex_data.c    mem.c      mem_sec.c  o_fopen.c  o_time.c    threads_win.c
	cpt_err.c   dllmain.c   getenv.c     mem_clr.c  o_dir.c    o_init.c   threads_none.c     uid.c
	cryptlib.c  ebcdic.c    init.c       mem_dbg.c  o_fips.c   o_str.c    threads_pthread.c
	core_namemap.c

	der_writer.c

	packet.c

	param_*.c

	params.c
	params_*.c

	punycode.c

	context.c
	initthread.c

	provider.c
	provider_*.c

	core_*
	passphrase.c
	sparse_*.c
	trace.c
)

list(FILTER SRCS EXCLUDE REGEX "evp/legacy_wp.*")
list(FILTER SRCS EXCLUDE REGEX "evp/legacy_md2.*")
list(FILTER SRCS EXCLUDE REGEX "evp/legacy_mdc2.*")
list(FILTER SRCS EXCLUDE REGEX "evp/legacy_md3.*")
list(FILTER SRCS EXCLUDE REGEX "evp/legacy_md4.*")
list(FILTER SRCS EXCLUDE REGEX "evp/legacy_meth.*")
list(FILTER SRCS EXCLUDE REGEX "ecp_nistz256_table.c")
list(FILTER SRCS EXCLUDE REGEX ".*_test_.*")
list(FILTER SRCS EXCLUDE REGEX "ncbc_enc.c")

list(APPEND SRCS 	self_test_core.c)

if (SHARED_OPENSSL)
add_library(crypto SHARED ${SRCS} ${ASYNC_ARCH})
else()
add_library(crypto STATIC ${SRCS} ${ASYNC_ARCH})
endif()

set_target_properties(crypto PROPERTIES FOLDER "third_party/openssl")

target_compile_definitions(crypto PUBLIC
	-DOPENSSL_NO_ASM
	-DOPENSSL_NO_ASYNC
	-DOPENSSL_NO_RC1
	-DOPENSSL_NO_RC3
	-DOPENSSL_NO_MD1
	-DOPENSSL_NO_MD2
	-DOPENSSL_NO_MD3
	-DOPENSSL_NO_MD4
	-DOPENSSL_NO_MDC2
	-DOPENSSL_NO_WHIRLPOOL
	-DOPENSSL_NO_COMP
	-DOPENSSL_NO_CHACHA
	-DOPENSSL_NO_TRACE
	-DOPENSSLDIR=""
	-DOPENSSL_NO_DEPRECATED_3_0
	-DWIN32_LEAN_AND_MEAN )

target_compile_definitions(crypto PRIVATE
	-DMODULESDIR=""
	-DENGINESDIR=""
	-DCRYPTO_memcmp=memcmp
)


target_include_directories(crypto PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include/)

# target_compile_definitions(crypto PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include/)

target_link_libraries(crypto openssl_providers openssl_common)

if (APPLE)
	target_compile_definitions(crypto PRIVATE -DASYNC_NULL)
endif()

if(NOT MSVC AND NOT APPLE)
	target_link_libraries(crypto PUBLIC)
endif()

