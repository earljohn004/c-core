/* -*- c-file-style:"stroustrup"; indent-tabs-mode: nil -*- */
#include "pbpal.h"

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/select.h>
#define SOCKET_ERROR -1
#endif

#include "pubnub_internal.h"
#include "pubnub_assert.h"

#include <openssl/pem.h>
#include <openssl/ssl.h>




#define HTTP_PORT_STRING "80"


static enum pubnub_res resolv_and_connect_wout_SSL(pubnub_t *pb)
{
    DEBUG_PRINTF("resolv_and_connect_wout_SSL\n");
    if (NULL == pb->pal.bio) {
        char const*origin = PUBNUB_ORIGIN_SETTABLE ? pb->origin : PUBNUB_ORIGIN;
        DEBUG_PRINTF("pb=%p: Don't have BIO\n", pb);
        pb->pal.bio = BIO_new_connect((char*)origin);
    }
    if (NULL == pb->pal.bio) {
        return PNR_ADDR_RESOLUTION_FAILED;
    }
    BIO_set_conn_port(pb->pal.bio, HTTP_PORT_STRING);
    
    BIO_set_nbio(pb->pal.bio, !pb->options.use_blocking_io);
    
    WATCH(pb->options.use_blocking_io, "%d");
    if (BIO_do_connect(pb->pal.bio) <= 0) {
        if (BIO_should_retry(pb->pal.bio)) {
            return (pbntf_got_socket(pb, pb->pal.bio) < 0) ? PNR_CONNECT_FAILED : PNR_IN_PROGRESS;
        }
        ERR_print_errors_fp(stderr);
        BIO_free_all(pb->pal.bio);
		pb->pal.bio = NULL;
        DEBUG_PRINTF("BIO_do_connect failed\n");
        return PNR_ADDR_RESOLUTION_FAILED;
    }

    DEBUG_PRINTF("pb=%p: BIO connected\n", pb);
    
    switch (pbntf_got_socket(pb, pb->pal.bio)) {
    case 0: return PNR_STARTED; /* Should really be PNR_OK, see below */
    case +1: return PNR_STARTED;
    case -1: default: return PNR_CONNECT_FAILED;
    }
    /* If we return PNR_OK, then the whole transaction can finish
       in one call to Netcore FSM. That would be nice, but some
       tests want to be able to cancel a request, which would
       then be impossible. So, until we figure out how to handle
       that, we shall return PNR_STARTED.
    */
}


/* Starfield Inc (Class 2) Root certificate.
   It was used to sign the server certificate of https://pubsub.pubnub.com.
   (at the time of writing this, 2015-06-04).
 */
static char pubnub_cert[] = "-----BEGIN CERTIFICATE-----\n"
"MIIEDzCCAvegAwIBAgIBADANBgkqhkiG9w0BAQUFADBoMQswCQYDVQQGEwJVUzEl\n"
"MCMGA1UEChMcU3RhcmZpZWxkIFRlY2hub2xvZ2llcywgSW5jLjEyMDAGA1UECxMp\n"
"U3RhcmZpZWxkIENsYXNzIDIgQ2VydGlmaWNhdGlvbiBBdXRob3JpdHkwHhcNMDQw\n"
"NjI5MTczOTE2WhcNMzQwNjI5MTczOTE2WjBoMQswCQYDVQQGEwJVUzElMCMGA1UE\n"
"ChMcU3RhcmZpZWxkIFRlY2hub2xvZ2llcywgSW5jLjEyMDAGA1UECxMpU3RhcmZp\n"
"ZWxkIENsYXNzIDIgQ2VydGlmaWNhdGlvbiBBdXRob3JpdHkwggEgMA0GCSqGSIb3\n"
"DQEBAQUAA4IBDQAwggEIAoIBAQC3Msj+6XGmBIWtDBFk385N78gDGIc/oav7PKaf\n"
"8MOh2tTYbitTkPskpD6E8J7oX+zlJ0T1KKY/e97gKvDIr1MvnsoFAZMej2YcOadN\n"
"+lq2cwQlZut3f+dZxkqZJRRU6ybH838Z1TBwj6+wRir/resp7defqgSHo9T5iaU0\n"
"X9tDkYI22WY8sbi5gv2cOj4QyDvvBmVmepsZGD3/cVE8MC5fvj13c7JdBmzDI1aa\n"
"K4UmkhynArPkPw2vCHmCuDY96pzTNbO8acr1zJ3o/WSNF4Azbl5KXZnJHoe0nRrA\n"
"1W4TNSNe35tfPe/W93bC6j67eA0cQmdrBNj41tpvi/JEoAGrAgEDo4HFMIHCMB0G\n"
"A1UdDgQWBBS/X7fRzt0fhvRbVazc1xDCDqmI5zCBkgYDVR0jBIGKMIGHgBS/X7fR\n"
"zt0fhvRbVazc1xDCDqmI56FspGowaDELMAkGA1UEBhMCVVMxJTAjBgNVBAoTHFN0\n"
"YXJmaWVsZCBUZWNobm9sb2dpZXMsIEluYy4xMjAwBgNVBAsTKVN0YXJmaWVsZCBD\n"
"bGFzcyAyIENlcnRpZmljYXRpb24gQXV0aG9yaXR5ggEAMAwGA1UdEwQFMAMBAf8w\n"
"DQYJKoZIhvcNAQEFBQADggEBAAWdP4id0ckaVaGsafPzWdqbAYcaT1epoXkJKtv3\n"
"L7IezMdeatiDh6GX70k1PncGQVhiv45YuApnP+yz3SFmH8lU+nLMPUxA2IGvd56D\n"
"eruix/U0F47ZEUD0/CwqTRV/p2JdLiXTAAsgGh1o+Re49L2L7ShZ3U0WixeDyLJl\n"
"xy16paq8U4Zt3VekyvggQQto8PT7dL5WXXp59fkdheMtlb71cZBDzI0fmgAKhynp\n"
"VSJYACPq4xJDKVtHCN2MQWplBqjlIapBtJUhlbl90TSrE9atvNziPTnNvT51cKEY\n"
"WQPJIrSPnNVeKtelttQKbfi3QBFGmh95DmK/D5fs4C8fF5Q=\n"
    "-----END CERTIFICATE-----\n";


static int add_pubnub_cert(SSL_CTX *sslCtx)
{
    X509 *cert;
    BIO *mem;
 
    mem = BIO_new(BIO_s_mem());
    BIO_puts(mem, pubnub_cert);
    cert = PEM_read_bio_X509(mem, NULL, 0, NULL);
    BIO_free(mem); 
    
    // set certificate to sslCtx
    X509_STORE_add_cert(SSL_CTX_get_cert_store(sslCtx), cert);

    /* In principle, it would be nice to use this instead, if we
       had a way to find out what is the file and/or path
       of the trusted root certificates. It would fix a problem
       that would happen if the root certificate used on
       `https://pubsub.pubnub.com` changes...

       One way to "fix" this would be to add some #defines
       for these, though it's not a user-friendly solution, so
       we're not doing it for now.

//    SSL_CTX_load_verify_locations(sslCtx, NULL, "/etc/ssl/certs");
    */

    return 0;
}


enum pubnub_res pbpal_resolv_and_connect(pubnub_t *pb)
{
    SSL *ssl;
    int rslt;
    char const* origin = PUBNUB_ORIGIN_SETTABLE ? pb->origin : PUBNUB_ORIGIN;
        
    PUBNUB_ASSERT(pb_valid_ctx_ptr(pb));
    PUBNUB_ASSERT_OPT((pb->state == PBS_IDLE) || (pb->state == PBS_WAIT_DNS));

    if (!pb->options.useSSL) {
        return resolv_and_connect_wout_SSL(pb);
    }
    
    if (NULL == pb->pal.ctx) {
        DEBUG_PRINTF("pb=%p: Don't have SSL_CTX\n", pb);
        pb->pal.ctx = SSL_CTX_new(SSLv23_client_method());
        if (NULL == pb->pal.ctx) {
            ERR_print_errors_fp(stderr);
            DEBUG_PRINTF("pb=%p SSL_CTX_new failed\n", pb);
            return PNR_ADDR_RESOLUTION_FAILED;
        }
        DEBUG_PRINTF("pb=%p: Got SSL_CTX\n", pb);
        add_pubnub_cert(pb->pal.ctx);
    }
    
    if (NULL == pb->pal.bio) {
        DEBUG_PRINTF("pb=%p: Don't have BIO\n", pb);
        pb->pal.bio = BIO_new_ssl_connect(pb->pal.ctx);
    }
    if (NULL == pb->pal.bio) {
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(pb->pal.ctx);
        pb->pal.ctx = NULL;
        return PNR_ADDR_RESOLUTION_FAILED;
    }
    
    DEBUG_PRINTF("pb=%p: Got BIO_new_ssl\n", pb);
    
    BIO_get_ssl(pb->pal.bio, &ssl);
    SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY); /* maybe not auto_retry? */

    BIO_set_conn_hostname(pb->pal.bio, origin);
    BIO_set_conn_port(pb->pal.bio, "https");

    BIO_set_nbio(pb->pal.bio, !pb->options.use_blocking_io);

    WATCH(pb->options.use_blocking_io, "%d");
    if (BIO_do_connect(pb->pal.bio) <= 0) {
        if (BIO_should_retry(pb->pal.bio)) {
            DEBUG_PRINTF("BIO_should_retry\n");
            return (pbntf_got_socket(pb, pb->pal.bio) < 0) ? PNR_CONNECT_FAILED : PNR_IN_PROGRESS;
        }
        ERR_print_errors_fp(stderr);
        BIO_free_all(pb->pal.bio);
        pb->pal.bio = NULL;
        SSL_CTX_free(pb->pal.ctx);
        pb->pal.ctx = NULL;
        DEBUG_PRINTF("BIO_do_connect failed\n");
        return PNR_ADDR_RESOLUTION_FAILED;
    }

    DEBUG_PRINTF("pb=%p: BIO connected\n", pb);

    rslt = SSL_get_verify_result(ssl);
    if (rslt != X509_V_OK) {
        DEBUG_PRINTF("SSL_get_verify_result() failed == %d(%s)\n", rslt, X509_verify_cert_error_string(rslt));
        ERR_print_errors_fp(stderr);
        BIO_free_all(pb->pal.bio);
        pb->pal.bio = NULL;
        SSL_CTX_free(pb->pal.ctx);
        pb->pal.ctx = NULL;
        if (pb->options.fallbackSSL) {
            return resolv_and_connect_wout_SSL(pb);
        }
        return PNR_CONNECT_FAILED;
    }

    switch (pbntf_got_socket(pb, pb->pal.bio)) {
    case 0: return PNR_STARTED; /* see pubnub_res resolv_and_connect_wout_SSL */
    case +1: return PNR_STARTED;
    case -1: default: return PNR_CONNECT_FAILED;
    }
}


enum pubnub_res pbpal_check_resolv_and_connect(pubnub_t *pb)
{
    fd_set read_set, write_set;
    int socket;
    int rslt;
    struct timeval timev = { 0, 300000 };
    
    if (-1 == BIO_get_fd(pb->pal.bio, &socket)) {
        DEBUG_PRINTF("Uninitialized BIO!\n");
        return PNR_CONNECT_FAILED;
    }
    FD_ZERO(&read_set);
    FD_ZERO(&write_set);
    FD_SET(socket, &read_set);
    FD_SET(socket, &write_set);
    rslt = select(socket + 1, &read_set, &write_set, NULL, &timev);
    if (SOCKET_ERROR == rslt) {
        DEBUG_PRINTF("select() Error!\n");
        return PNR_CONNECT_FAILED;
    }
    else if (rslt > 0) {
        DEBUG_PRINTF("select() event\n");
        return pbpal_resolv_and_connect(pb);
    }
    DEBUG_PRINTF("no select() events\n");
    return PNR_IN_PROGRESS;
}
