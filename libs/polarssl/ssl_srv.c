/*
 *  SSLv3/TLSv1 server-side functions
 *
 *  Copyright (C) 2006-2014, Brainspark B.V.
 *
 *  This file is part of PolarSSL (http://www.polarssl.org)
 *  Lead Maintainer: Paul Bakker <polarssl_maintainer at polarssl.org>
 *
 *  All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "polarssl.h"

#if defined(POLARSSL_SSL_SRV_C)

#include "debug.h"
#include "ssl.h"

#define polarssl_malloc     malloc
#define polarssl_free       free

#include <stdlib.h>
#include <stdio.h>

static int ssl_parse_renegotiation_info( ssl_context *ssl,
                                         const unsigned char *buf,
                                         size_t len )
{
    int ret;

    if( ssl->renegotiation == SSL_INITIAL_HANDSHAKE )
    {
        if( len != 1 || buf[0] != 0x0 )
        {
            SSL_DEBUG_MSG( 1, ( "non-zero length renegotiated connection field" ) );

            if( ( ret = ssl_send_fatal_handshake_failure( ssl ) ) != 0 )
                return( ret );

            return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_HELLO );
        }

        ssl->secure_renegotiation = SSL_SECURE_RENEGOTIATION;
    }
    else
    {
        /* Check verify-data in constant-time. The length OTOH is no secret */
        if( len    != 1 + ssl->verify_data_len ||
            buf[0] !=     ssl->verify_data_len ||
            safer_memcmp( buf + 1, ssl->peer_verify_data,
                          ssl->verify_data_len ) != 0 )
        {
            SSL_DEBUG_MSG( 1, ( "non-matching renegotiated connection field" ) );

            if( ( ret = ssl_send_fatal_handshake_failure( ssl ) ) != 0 )
                return( ret );

            return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_HELLO );
        }
    }

    return( 0 );
}

#if defined(POLARSSL_SSL_PROTO_TLS1_2)
static int ssl_parse_signature_algorithms_ext( ssl_context *ssl,
                                               const unsigned char *buf,
                                               size_t len )
{
    size_t sig_alg_list_size;
    const unsigned char *p;

    sig_alg_list_size = ( ( buf[0] << 8 ) | ( buf[1] ) );
    if( sig_alg_list_size + 2 != len ||
        sig_alg_list_size %2 != 0 )
    {
        SSL_DEBUG_MSG( 1, ( "bad client hello message" ) );
        return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_HELLO );
    }

    p = buf + 2;
    while( sig_alg_list_size > 0 )
    {
        /*
         * For now, just ignore signature algorithm and rely on offered
         * ciphersuites only. To be fixed later.
         */
#if defined(POLARSSL_SHA512_C)
        if( p[0] == SSL_HASH_SHA512 )
        {
            ssl->handshake->sig_alg = SSL_HASH_SHA512;
            break;
        }
        if( p[0] == SSL_HASH_SHA384 )
        {
            ssl->handshake->sig_alg = SSL_HASH_SHA384;
            break;
        }
#endif
#if defined(POLARSSL_SHA256_C)
        if( p[0] == SSL_HASH_SHA256 )
        {
            ssl->handshake->sig_alg = SSL_HASH_SHA256;
            break;
        }
        if( p[0] == SSL_HASH_SHA224 )
        {
            ssl->handshake->sig_alg = SSL_HASH_SHA224;
            break;
        }
#endif
        if( p[0] == SSL_HASH_SHA1 )
        {
            ssl->handshake->sig_alg = SSL_HASH_SHA1;
            break;
        }
        if( p[0] == SSL_HASH_MD5 )
        {
            ssl->handshake->sig_alg = SSL_HASH_MD5;
            break;
        }

        sig_alg_list_size -= 2;
        p += 2;
    }

    SSL_DEBUG_MSG( 3, ( "client hello v3, signature_algorithm ext: %d",
                   ssl->handshake->sig_alg ) );

    return( 0 );
}
#endif /* POLARSSL_SSL_PROTO_TLS1_2 */

#if defined(POLARSSL_ECDH_C) || defined(POLARSSL_ECDSA_C)
static int ssl_parse_supported_elliptic_curves( ssl_context *ssl,
                                                const unsigned char *buf,
                                                size_t len )
{
    size_t list_size, our_size;
    const unsigned char *p;
    const ecp_curve_info *curve_info, **curves;

    list_size = ( ( buf[0] << 8 ) | ( buf[1] ) );
    if( list_size + 2 != len ||
        list_size % 2 != 0 )
    {
        SSL_DEBUG_MSG( 1, ( "bad client hello message" ) );
        return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_HELLO );
    }

    /* Don't allow our peer to make us allocate too much memory,
     * and leave room for a final 0 */
    our_size = list_size / 2 + 1;
    if( our_size > POLARSSL_ECP_DP_MAX )
        our_size = POLARSSL_ECP_DP_MAX;

    if( ( curves = polarssl_malloc( our_size * sizeof( *curves ) ) ) == NULL )
        return( POLARSSL_ERR_SSL_MALLOC_FAILED );

	/* explicit void pointer cast for buggy MS compiler */
    memset( (void *) curves, 0, our_size * sizeof( *curves ) );
    ssl->handshake->curves = curves;

    p = buf + 2;
    while( list_size > 0 && our_size > 1 )
    {
        curve_info = ecp_curve_info_from_tls_id( ( p[0] << 8 ) | p[1] );

        if( curve_info != NULL )
        {
            *curves++ = curve_info;
            our_size--;
        }

        list_size -= 2;
        p += 2;
    }

    return( 0 );
}

static int ssl_parse_supported_point_formats( ssl_context *ssl,
                                              const unsigned char *buf,
                                              size_t len )
{
    size_t list_size;
    const unsigned char *p;

    list_size = buf[0];
    if( list_size + 1 != len )
    {
        SSL_DEBUG_MSG( 1, ( "bad client hello message" ) );
        return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_HELLO );
    }

    p = buf + 2;
    while( list_size > 0 )
    {
        if( p[0] == POLARSSL_ECP_PF_UNCOMPRESSED ||
            p[0] == POLARSSL_ECP_PF_COMPRESSED )
        {
            ssl->handshake->ecdh_ctx.point_format = p[0];
            SSL_DEBUG_MSG( 4, ( "point format selected: %d", p[0] ) );
            return( 0 );
        }

        list_size--;
        p++;
    }

    return( 0 );
}
#endif /* POLARSSL_ECDH_C || POLARSSL_ECDSA_C */

/*
 * Auxiliary functions for ServerHello parsing and related actions
 */

#if defined(POLARSSL_X509_CRT_PARSE_C)
/*
 * Return 1 if the given EC key uses the given curve, 0 otherwise
 */
#if defined(POLARSSL_ECDSA_C)
static int ssl_key_matches_curves( pk_context *pk,
                                   const ecp_curve_info **curves )
{
    const ecp_curve_info **crv = curves;
    ecp_group_id grp_id = pk_ec( *pk )->grp.id;

    while( *crv != NULL )
    {
        if( (*crv)->grp_id == grp_id )
            return( 1 );
        crv++;
    }

    return( 0 );
}
#endif /* POLARSSL_ECDSA_C */

/*
 * Try picking a certificate for this ciphersuite,
 * return 0 on success and -1 on failure.
 */
static int ssl_pick_cert( ssl_context *ssl,
                          const ssl_ciphersuite_t * ciphersuite_info )
{
    ssl_key_cert *cur, *list;
    pk_type_t pk_alg = ssl_get_ciphersuite_sig_pk_alg( ciphersuite_info );

        list = ssl->handshake->key_cert;

    if( pk_alg == POLARSSL_PK_NONE )
        return( 0 );

    for( cur = list; cur != NULL; cur = cur->next )
    {
        if( ! pk_can_do( cur->key, pk_alg ) )
            continue;

        /*
         * This avoids sending the client a cert it'll reject based on
         * keyUsage or other extensions.
         *
         * It also allows the user to provision different certificates for
         * different uses based on keyUsage, eg if they want to avoid signing
         * and decrypting with the same RSA key.
         */
        if( ssl_check_cert_usage( cur->cert, ciphersuite_info,
                                  SSL_IS_SERVER ) != 0 )
        {
            continue;
        }

#if defined(POLARSSL_ECDSA_C)
        if( pk_alg == POLARSSL_PK_ECDSA )
        {
            if( ssl_key_matches_curves( cur->key, ssl->handshake->curves ) )
                break;
        }
        else
#endif
            break;
    }

    if( cur == NULL )
        return( -1 );

    ssl->handshake->key_cert = cur;
    return( 0 );
}
#endif /* POLARSSL_X509_CRT_PARSE_C */

/*
 * Check if a given ciphersuite is suitable for use with our config/keys/etc
 * Sets ciphersuite_info only if the suite matches.
 */
static int ssl_ciphersuite_match( ssl_context *ssl, int suite_id,
                                  const ssl_ciphersuite_t **ciphersuite_info )
{
    const ssl_ciphersuite_t *suite_info;

    suite_info = ssl_ciphersuite_from_id( suite_id );
    if( suite_info == NULL )
    {
        SSL_DEBUG_MSG( 1, ( "ciphersuite info for %04x not found", suite_id ) );
        return( POLARSSL_ERR_SSL_BAD_INPUT_DATA );
    }

    if( suite_info->min_minor_ver > ssl->minor_ver ||
        suite_info->max_minor_ver < ssl->minor_ver )
        return( 0 );

#if defined(POLARSSL_ECDH_C) || defined(POLARSSL_ECDSA_C)
    if( ssl_ciphersuite_uses_ec( suite_info ) &&
        ( ssl->handshake->curves == NULL ||
          ssl->handshake->curves[0] == NULL ) )
        return( 0 );
#endif

#if defined(POLARSSL_KEY_EXCHANGE__SOME__PSK_ENABLED)
    /* If the ciphersuite requires a pre-shared key and we don't
     * have one, skip it now rather than failing later */
    if( ssl_ciphersuite_uses_psk( suite_info ) &&
        ssl->f_psk == NULL &&
        ( ssl->psk == NULL || ssl->psk_identity == NULL ||
          ssl->psk_identity_len == 0 || ssl->psk_len == 0 ) )
        return( 0 );
#endif

#if defined(POLARSSL_X509_CRT_PARSE_C)
    /*
     * Final check: if ciphersuite requires us to have a
     * certificate/key of a particular type:
     * - select the appropriate certificate if we have one, or
     * - try the next ciphersuite if we don't
     * This must be done last since we modify the key_cert list.
     */
    if( ssl_pick_cert( ssl, suite_info ) != 0 )
        return( 0 );
#endif

    *ciphersuite_info = suite_info;
    return( 0 );
}

static int ssl_parse_client_hello( ssl_context *ssl )
{
    int ret;
    unsigned int i, j;
    size_t n;
    unsigned int ciph_len, sess_len;
    unsigned int comp_len;
    unsigned int ext_len = 0;
    unsigned char *buf, *p, *ext;
    int renegotiation_info_seen = 0;
    int handshake_failure = 0;
    const int *ciphersuites;
    const ssl_ciphersuite_t *ciphersuite_info;

    SSL_DEBUG_MSG( 2, ( "=> parse client hello" ) );

    if( ssl->renegotiation == SSL_INITIAL_HANDSHAKE &&
        ( ret = ssl_fetch_input( ssl, 5 ) ) != 0 )
    {
        SSL_DEBUG_RET( 1, "ssl_fetch_input", ret );
        return( ret );
    }

    buf = ssl->in_hdr;


    SSL_DEBUG_BUF( 4, "record header", buf, 5 );

    SSL_DEBUG_MSG( 3, ( "client hello v3, message type: %d",
                   buf[0] ) );
    SSL_DEBUG_MSG( 3, ( "client hello v3, message len.: %d",
                   ( buf[3] << 8 ) | buf[4] ) );
    SSL_DEBUG_MSG( 3, ( "client hello v3, protocol ver: [%d:%d]",
                   buf[1], buf[2] ) );

    /*
     * SSLv3/TLS Client Hello
     *
     * Record layer:
     *     0  .   0   message type
     *     1  .   2   protocol version
     *     3  .   4   message length
     */

    /* According to RFC 5246 Appendix E.1, the version here is typically
     * "{03,00}, the lowest version number supported by the client, [or] the
     * value of ClientHello.client_version", so the only meaningful check here
     * is the major version shouldn't be less than 3 */
    if( buf[0] != SSL_MSG_HANDSHAKE ||
        buf[1] < SSL_MAJOR_VERSION_3 )
    {
        SSL_DEBUG_MSG( 1, ( "bad client hello message" ) );
        return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_HELLO );
    }

    n = ( buf[3] << 8 ) | buf[4];

    if( n < 45 || n > 2048 )
    {
        SSL_DEBUG_MSG( 1, ( "bad client hello message" ) );
        return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_HELLO );
    }

    if( ssl->renegotiation == SSL_INITIAL_HANDSHAKE &&
        ( ret = ssl_fetch_input( ssl, 5 + n ) ) != 0 )
    {
        SSL_DEBUG_RET( 1, "ssl_fetch_input", ret );
        return( ret );
    }

    buf = ssl->in_msg;
    if( !ssl->renegotiation )
        n = ssl->in_left - 5;
    else
        n = ssl->in_msglen;

    ssl->handshake->update_checksum( ssl, buf, n );

    /*
     * SSL layer:
     *     0  .   0   handshake type
     *     1  .   3   handshake length
     *     4  .   5   protocol version
     *     6  .   9   UNIX time()
     *    10  .  37   random bytes
     *    38  .  38   session id length
     *    39  . 38+x  session id
     *   39+x . 40+x  ciphersuitelist length
     *   41+x .  ..   ciphersuitelist
     *    ..  .  ..   compression alg.
     *    ..  .  ..   extensions
     */
    SSL_DEBUG_BUF( 4, "record contents", buf, n );

    SSL_DEBUG_MSG( 3, ( "client hello v3, handshake type: %d",
                   buf[0] ) );
    SSL_DEBUG_MSG( 3, ( "client hello v3, handshake len.: %d",
                   ( buf[1] << 16 ) | ( buf[2] << 8 ) | buf[3] ) );
    SSL_DEBUG_MSG( 3, ( "client hello v3, max. version: [%d:%d]",
                   buf[4], buf[5] ) );

    /*
     * Check the handshake type and protocol version
     */
    if( buf[0] != SSL_HS_CLIENT_HELLO )
    {
        SSL_DEBUG_MSG( 1, ( "bad client hello message" ) );
        return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_HELLO );
    }

    ssl->major_ver = buf[4];
    ssl->minor_ver = buf[5];

    ssl->handshake->max_major_ver = ssl->major_ver;
    ssl->handshake->max_minor_ver = ssl->minor_ver;

    if( ssl->major_ver < ssl->min_major_ver ||
        ssl->minor_ver < ssl->min_minor_ver )
    {
        SSL_DEBUG_MSG( 1, ( "client only supports ssl smaller than minimum"
                            " [%d:%d] < [%d:%d]",
                            ssl->major_ver, ssl->minor_ver,
                            ssl->min_major_ver, ssl->min_minor_ver ) );

        ssl_send_alert_message( ssl, SSL_ALERT_LEVEL_FATAL,
                                     SSL_ALERT_MSG_PROTOCOL_VERSION );

        return( POLARSSL_ERR_SSL_BAD_HS_PROTOCOL_VERSION );
    }

    if( ssl->major_ver > ssl->max_major_ver )
    {
        ssl->major_ver = ssl->max_major_ver;
        ssl->minor_ver = ssl->max_minor_ver;
    }
    else if( ssl->minor_ver > ssl->max_minor_ver )
        ssl->minor_ver = ssl->max_minor_ver;

    memcpy( ssl->handshake->randbytes, buf + 6, 32 );

    /*
     * Check the handshake message length
     */
    if( buf[1] != 0 || n != (unsigned int) 4 + ( ( buf[2] << 8 ) | buf[3] ) )
    {
        SSL_DEBUG_MSG( 1, ( "bad client hello message" ) );
        return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_HELLO );
    }

    /*
     * Check the session length
     */
    sess_len = buf[38];

    if( sess_len > 32 )
    {
        SSL_DEBUG_MSG( 1, ( "bad client hello message" ) );
        return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_HELLO );
    }

    ssl->session_negotiate->length = sess_len;
    memset( ssl->session_negotiate->id, 0,
            sizeof( ssl->session_negotiate->id ) );
    memcpy( ssl->session_negotiate->id, buf + 39,
            ssl->session_negotiate->length );

    /*
     * Check the ciphersuitelist length
     */
    ciph_len = ( buf[39 + sess_len] << 8 )
             | ( buf[40 + sess_len]      );

    if( ciph_len < 2 || ciph_len > 256 || ( ciph_len % 2 ) != 0 )
    {
        SSL_DEBUG_MSG( 1, ( "bad client hello message" ) );
        return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_HELLO );
    }

    /*
     * Check the compression algorithms length
     */
    comp_len = buf[41 + sess_len + ciph_len];

    if( comp_len < 1 || comp_len > 16 )
    {
        SSL_DEBUG_MSG( 1, ( "bad client hello message" ) );
        return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_HELLO );
    }

    /*
     * Check the extension length
     */
    if( n > 42 + sess_len + ciph_len + comp_len )
    {
        ext_len = ( buf[42 + sess_len + ciph_len + comp_len] << 8 )
                | ( buf[43 + sess_len + ciph_len + comp_len]      );

        if( ( ext_len > 0 && ext_len < 4 ) ||
            n != 44 + sess_len + ciph_len + comp_len + ext_len )
        {
            SSL_DEBUG_MSG( 1, ( "bad client hello message" ) );
            SSL_DEBUG_BUF( 3, "Ext", buf + 44 + sess_len + ciph_len + comp_len, ext_len);
            return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_HELLO );
        }
    }

    ssl->session_negotiate->compression = SSL_COMPRESS_NULL;

    SSL_DEBUG_BUF( 3, "client hello, random bytes",
                   buf +  6,  32 );
    SSL_DEBUG_BUF( 3, "client hello, session id",
                   buf + 38,  sess_len );
    SSL_DEBUG_BUF( 3, "client hello, ciphersuitelist",
                   buf + 41 + sess_len,  ciph_len );
    SSL_DEBUG_BUF( 3, "client hello, compression",
                   buf + 42 + sess_len + ciph_len, comp_len );

    /*
     * Check for TLS_EMPTY_RENEGOTIATION_INFO_SCSV
     */
    for( i = 0, p = buf + 41 + sess_len; i < ciph_len; i += 2, p += 2 )
    {
        if( p[0] == 0 && p[1] == SSL_EMPTY_RENEGOTIATION_INFO )
        {
            SSL_DEBUG_MSG( 3, ( "received TLS_EMPTY_RENEGOTIATION_INFO " ) );
            if( ssl->renegotiation == SSL_RENEGOTIATION )
            {
                SSL_DEBUG_MSG( 1, ( "received RENEGOTIATION SCSV during renegotiation" ) );

                if( ( ret = ssl_send_fatal_handshake_failure( ssl ) ) != 0 )
                    return( ret );

                return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_HELLO );
            }
            ssl->secure_renegotiation = SSL_SECURE_RENEGOTIATION;
            break;
        }
    }

    ext = buf + 44 + sess_len + ciph_len + comp_len;

    while( ext_len )
    {
        unsigned int ext_id   = ( ( ext[0] <<  8 )
                                | ( ext[1]       ) );
        unsigned int ext_size = ( ( ext[2] <<  8 )
                                | ( ext[3]       ) );

        if( ext_size + 4 > ext_len )
        {
            SSL_DEBUG_MSG( 1, ( "bad client hello message" ) );
            return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_HELLO );
        }
        switch( ext_id )
        {
        case TLS_EXT_RENEGOTIATION_INFO:
            SSL_DEBUG_MSG( 3, ( "found renegotiation extension" ) );
            renegotiation_info_seen = 1;

            ret = ssl_parse_renegotiation_info( ssl, ext + 4, ext_size );
            if( ret != 0 )
                return( ret );
            break;

#if defined(POLARSSL_SSL_PROTO_TLS1_2)
        case TLS_EXT_SIG_ALG:
            SSL_DEBUG_MSG( 3, ( "found signature_algorithms extension" ) );
            if( ssl->renegotiation == SSL_RENEGOTIATION )
                break;

            ret = ssl_parse_signature_algorithms_ext( ssl, ext + 4, ext_size );
            if( ret != 0 )
                return( ret );
            break;
#endif /* POLARSSL_SSL_PROTO_TLS1_2 */

#if defined(POLARSSL_ECDH_C) || defined(POLARSSL_ECDSA_C)
        case TLS_EXT_SUPPORTED_ELLIPTIC_CURVES:
            SSL_DEBUG_MSG( 3, ( "found supported elliptic curves extension" ) );

            ret = ssl_parse_supported_elliptic_curves( ssl, ext + 4, ext_size );
            if( ret != 0 )
                return( ret );
            break;

        case TLS_EXT_SUPPORTED_POINT_FORMATS:
            SSL_DEBUG_MSG( 3, ( "found supported point formats extension" ) );
            ssl->handshake->cli_exts |= TLS_EXT_SUPPORTED_POINT_FORMATS_PRESENT;

            ret = ssl_parse_supported_point_formats( ssl, ext + 4, ext_size );
            if( ret != 0 )
                return( ret );
            break;
#endif /* POLARSSL_ECDH_C || POLARSSL_ECDSA_C */

        default:
            SSL_DEBUG_MSG( 3, ( "unknown extension found: %d (ignoring)",
                           ext_id ) );
        }

        ext_len -= 4 + ext_size;
        ext += 4 + ext_size;

        if( ext_len > 0 && ext_len < 4 )
        {
            SSL_DEBUG_MSG( 1, ( "bad client hello message" ) );
            return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_HELLO );
        }
    }

    /*
     * Renegotiation security checks
     */
    if( ssl->secure_renegotiation == SSL_LEGACY_RENEGOTIATION &&
        ssl->allow_legacy_renegotiation == SSL_LEGACY_BREAK_HANDSHAKE )
    {
        SSL_DEBUG_MSG( 1, ( "legacy renegotiation, breaking off handshake" ) );
        handshake_failure = 1;
    }
    else if( ssl->renegotiation == SSL_RENEGOTIATION &&
             ssl->secure_renegotiation == SSL_SECURE_RENEGOTIATION &&
             renegotiation_info_seen == 0 )
    {
        SSL_DEBUG_MSG( 1, ( "renegotiation_info extension missing (secure)" ) );
        handshake_failure = 1;
    }
    else if( ssl->renegotiation == SSL_RENEGOTIATION &&
             ssl->secure_renegotiation == SSL_LEGACY_RENEGOTIATION &&
             ssl->allow_legacy_renegotiation == SSL_LEGACY_NO_RENEGOTIATION )
    {
        SSL_DEBUG_MSG( 1, ( "legacy renegotiation not allowed" ) );
        handshake_failure = 1;
    }
    else if( ssl->renegotiation == SSL_RENEGOTIATION &&
             ssl->secure_renegotiation == SSL_LEGACY_RENEGOTIATION &&
             renegotiation_info_seen == 1 )
    {
        SSL_DEBUG_MSG( 1, ( "renegotiation_info extension present (legacy)" ) );
        handshake_failure = 1;
    }

    if( handshake_failure == 1 )
    {
        if( ( ret = ssl_send_fatal_handshake_failure( ssl ) ) != 0 )
            return( ret );

        return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_HELLO );
    }

    /*
     * Search for a matching ciphersuite
     * (At the end because we need information from the EC-based extensions
     * and certificate from the SNI callback triggered by the SNI extension.)
     */
    ciphersuites = ssl->ciphersuite_list[ssl->minor_ver];
    ciphersuite_info = NULL;
    for( i = 0; ciphersuites[i] != 0; i++ )
    {
        for( j = 0, p = buf + 41 + sess_len; j < ciph_len; j += 2, p += 2 )
        {
            if( p[0] != ( ( ciphersuites[i] >> 8 ) & 0xFF ) ||
                p[1] != ( ( ciphersuites[i]      ) & 0xFF ) )
                continue;

            if( ( ret = ssl_ciphersuite_match( ssl, ciphersuites[i],
                                               &ciphersuite_info ) ) != 0 )
                return( ret );

            if( ciphersuite_info != NULL )
                goto have_ciphersuite;
        }
    }

    SSL_DEBUG_MSG( 1, ( "got no ciphersuites in common" ) );

    if( ( ret = ssl_send_fatal_handshake_failure( ssl ) ) != 0 )
        return( ret );

    return( POLARSSL_ERR_SSL_NO_CIPHER_CHOSEN );

have_ciphersuite:
    ssl->session_negotiate->ciphersuite = ciphersuites[i];
    ssl->transform_negotiate->ciphersuite_info = ciphersuite_info;
    ssl_optimize_checksum( ssl, ssl->transform_negotiate->ciphersuite_info );

    ssl->in_left = 0;
    ssl->state++;

    SSL_DEBUG_MSG( 2, ( "<= parse client hello" ) );

    return( 0 );
}

static void ssl_write_renegotiation_ext( ssl_context *ssl,
                                         unsigned char *buf,
                                         size_t *olen )
{
    unsigned char *p = buf;

    if( ssl->secure_renegotiation != SSL_SECURE_RENEGOTIATION )
    {
        *olen = 0;
        return;
    }

    SSL_DEBUG_MSG( 3, ( "server hello, secure renegotiation extension" ) );

    *p++ = (unsigned char)( ( TLS_EXT_RENEGOTIATION_INFO >> 8 ) & 0xFF );
    *p++ = (unsigned char)( ( TLS_EXT_RENEGOTIATION_INFO      ) & 0xFF );

    *p++ = 0x00;
    *p++ = ( ssl->verify_data_len * 2 + 1 ) & 0xFF;
    *p++ = ssl->verify_data_len * 2 & 0xFF;

    memcpy( p, ssl->peer_verify_data, ssl->verify_data_len );
    p += ssl->verify_data_len;
    memcpy( p, ssl->own_verify_data, ssl->verify_data_len );
    p += ssl->verify_data_len;

    *olen = 5 + ssl->verify_data_len * 2;
}

#if defined(POLARSSL_ECDH_C) || defined(POLARSSL_ECDSA_C)
static void ssl_write_supported_point_formats_ext( ssl_context *ssl,
                                                   unsigned char *buf,
                                                   size_t *olen )
{
    unsigned char *p = buf;
    ((void) ssl);

    if( ( ssl->handshake->cli_exts &
          TLS_EXT_SUPPORTED_POINT_FORMATS_PRESENT ) == 0 )
    {
        *olen = 0;
        return;
    }

    SSL_DEBUG_MSG( 3, ( "server hello, supported_point_formats extension" ) );

    *p++ = (unsigned char)( ( TLS_EXT_SUPPORTED_POINT_FORMATS >> 8 ) & 0xFF );
    *p++ = (unsigned char)( ( TLS_EXT_SUPPORTED_POINT_FORMATS      ) & 0xFF );

    *p++ = 0x00;
    *p++ = 2;

    *p++ = 1;
    *p++ = POLARSSL_ECP_PF_UNCOMPRESSED;

    *olen = 6;
}
#endif /* POLARSSL_ECDH_C || POLARSSL_ECDSA_C */

static int ssl_write_server_hello( ssl_context *ssl )
{
    int ret;
    size_t olen, ext_len = 0, n;
    unsigned char *buf, *p;

    SSL_DEBUG_MSG( 2, ( "=> write server hello" ) );

    if( ssl->f_rng == NULL )
    {
        SSL_DEBUG_MSG( 1, ( "no RNG provided") );
        return( POLARSSL_ERR_SSL_NO_RNG );
    }

    /*
     *     0  .   0   handshake type
     *     1  .   3   handshake length
     *     4  .   5   protocol version
     *     6  .   9   UNIX time()
     *    10  .  37   random bytes
     */
    buf = ssl->out_msg;
    p = buf + 4;

    *p++ = (unsigned char) ssl->major_ver;
    *p++ = (unsigned char) ssl->minor_ver;

    SSL_DEBUG_MSG( 3, ( "server hello, chosen version: [%d:%d]",
                   buf[4], buf[5] ) );

    if( ( ret = ssl->f_rng( ssl->p_rng, p, 4 ) ) != 0 )
        return( ret );

    p += 4;

    if( ( ret = ssl->f_rng( ssl->p_rng, p, 28 ) ) != 0 )
        return( ret );

    p += 28;

    memcpy( ssl->handshake->randbytes + 32, buf + 6, 32 );

    SSL_DEBUG_BUF( 3, "server hello, random bytes", buf + 6, 32 );

    /*
     * Resume is 0  by default, see ssl_handshake_init().
     * It may be already set to 1 by ssl_parse_session_ticket_ext().
     * If not, try looking up session ID in our cache.
     */
    if( ssl->handshake->resume == 0 &&
        ssl->renegotiation == SSL_INITIAL_HANDSHAKE &&
        ssl->session_negotiate->length != 0 &&
        ssl->f_get_cache != NULL &&
        ssl->f_get_cache( ssl->p_get_cache, ssl->session_negotiate ) == 0 )
    {
        SSL_DEBUG_MSG( 3, ( "session successfully restored from cache" ) );
        ssl->handshake->resume = 1;
    }

    if( ssl->handshake->resume == 0 )
    {
        /*
         * New session, create a new session id,
         * unless we're about to issue a session ticket
         */
        ssl->state++;

        {
            ssl->session_negotiate->length = n = 32;
            if( ( ret = ssl->f_rng( ssl->p_rng, ssl->session_negotiate->id,
                                    n ) ) != 0 )
                return( ret );
        }
    }
    else
    {
        /*
         * Resuming a session
         */
        n = ssl->session_negotiate->length;
        ssl->state = SSL_SERVER_CHANGE_CIPHER_SPEC;

        if( ( ret = ssl_derive_keys( ssl ) ) != 0 )
        {
            SSL_DEBUG_RET( 1, "ssl_derive_keys", ret );
            return( ret );
        }
    }

    /*
     *    38  .  38     session id length
     *    39  . 38+n    session id
     *   39+n . 40+n    chosen ciphersuite
     *   41+n . 41+n    chosen compression alg.
     *   42+n . 43+n    extensions length
     *   44+n . 43+n+m  extensions
     */
    *p++ = (unsigned char) ssl->session_negotiate->length;
    memcpy( p, ssl->session_negotiate->id, ssl->session_negotiate->length );
    p += ssl->session_negotiate->length;

    SSL_DEBUG_MSG( 3, ( "server hello, session id len.: %d", n ) );
    SSL_DEBUG_BUF( 3,   "server hello, session id", buf + 39, n );
    SSL_DEBUG_MSG( 3, ( "%s session has been resumed",
                   ssl->handshake->resume ? "a" : "no" ) );

    *p++ = (unsigned char)( ssl->session_negotiate->ciphersuite >> 8 );
    *p++ = (unsigned char)( ssl->session_negotiate->ciphersuite      );
    *p++ = (unsigned char)( ssl->session_negotiate->compression      );

    SSL_DEBUG_MSG( 3, ( "server hello, chosen ciphersuite: %s",
           ssl_get_ciphersuite_name( ssl->session_negotiate->ciphersuite ) ) );
    SSL_DEBUG_MSG( 3, ( "server hello, compress alg.: 0x%02X",
                   ssl->session_negotiate->compression ) );

    /*
     *  First write extensions, then the total length
     */
    ssl_write_renegotiation_ext( ssl, p + 2 + ext_len, &olen );
    ext_len += olen;

#if defined(POLARSSL_ECDH_C) || defined(POLARSSL_ECDSA_C)
    ssl_write_supported_point_formats_ext( ssl, p + 2 + ext_len, &olen );
    ext_len += olen;
#endif

    SSL_DEBUG_MSG( 3, ( "server hello, total extension length: %d", ext_len ) );

    *p++ = (unsigned char)( ( ext_len >> 8 ) & 0xFF );
    *p++ = (unsigned char)( ( ext_len      ) & 0xFF );
    p += ext_len;

    ssl->out_msglen  = p - buf;
    ssl->out_msgtype = SSL_MSG_HANDSHAKE;
    ssl->out_msg[0]  = SSL_HS_SERVER_HELLO;

    ret = ssl_write_record( ssl );

    SSL_DEBUG_MSG( 2, ( "<= write server hello" ) );

    return( ret );
}

#if !defined(POLARSSL_KEY_EXCHANGE_RSA_ENABLED)       && \
    !defined(POLARSSL_KEY_EXCHANGE_DHE_RSA_ENABLED)   && \
    !defined(POLARSSL_KEY_EXCHANGE_ECDHE_RSA_ENABLED) && \
    !defined(POLARSSL_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED)
static int ssl_write_certificate_request( ssl_context *ssl )
{
    int ret = POLARSSL_ERR_SSL_FEATURE_UNAVAILABLE;
    const ssl_ciphersuite_t *ciphersuite_info = ssl->transform_negotiate->ciphersuite_info;

    SSL_DEBUG_MSG( 2, ( "=> write certificate request" ) );

    if( ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_PSK ||
        ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_RSA_PSK ||
        ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_DHE_PSK ||
        ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_ECDHE_PSK )
    {
        SSL_DEBUG_MSG( 2, ( "<= skip write certificate request" ) );
        ssl->state++;
        return( 0 );
    }

    SSL_DEBUG_MSG( 1, ( "should not happen" ) );
    return( ret );
}
#else
static int ssl_write_certificate_request( ssl_context *ssl )
{
    int ret = POLARSSL_ERR_SSL_FEATURE_UNAVAILABLE;
    const ssl_ciphersuite_t *ciphersuite_info = ssl->transform_negotiate->ciphersuite_info;
    size_t dn_size, total_dn_size; /* excluding length bytes */
    size_t ct_len, sa_len; /* including length bytes */
    unsigned char *buf, *p;
    const x509_crt *crt;

    SSL_DEBUG_MSG( 2, ( "=> write certificate request" ) );

    ssl->state++;

    if( ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_PSK ||
        ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_RSA_PSK ||
        ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_DHE_PSK ||
        ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_ECDHE_PSK ||
        ssl->authmode == SSL_VERIFY_NONE )
    {
        SSL_DEBUG_MSG( 2, ( "<= skip write certificate request" ) );
        return( 0 );
    }

    /*
     *     0  .   0   handshake type
     *     1  .   3   handshake length
     *     4  .   4   cert type count
     *     5  .. m-1  cert types
     *     m  .. m+1  sig alg length (TLS 1.2 only)
     *    m+1 .. n-1  SignatureAndHashAlgorithms (TLS 1.2 only) 
     *     n  .. n+1  length of all DNs
     *    n+2 .. n+3  length of DN 1
     *    n+4 .. ...  Distinguished Name #1
     *    ... .. ...  length of DN 2, etc.
     */
    buf = ssl->out_msg;
    p = buf + 4;

    /*
     * Supported certificate types
     *
     *     ClientCertificateType certificate_types<1..2^8-1>;
     *     enum { (255) } ClientCertificateType;
     */
    ct_len = 0;

#if defined(POLARSSL_RSA_C)
    p[1 + ct_len++] = SSL_CERT_TYPE_RSA_SIGN;
#endif
#if defined(POLARSSL_ECDSA_C)
    p[1 + ct_len++] = SSL_CERT_TYPE_ECDSA_SIGN;
#endif

    p[0] = (unsigned char) ct_len++;
    p += ct_len;

    sa_len = 0;
#if defined(POLARSSL_SSL_PROTO_TLS1_2)
    /*
     * Add signature_algorithms for verify (TLS 1.2)
     *
     *     SignatureAndHashAlgorithm supported_signature_algorithms<2..2^16-2>;
     *
     *     struct {
     *           HashAlgorithm hash;
     *           SignatureAlgorithm signature;
     *     } SignatureAndHashAlgorithm;
     *
     *     enum { (255) } HashAlgorithm;
     *     enum { (255) } SignatureAlgorithm;
     */
    if( ssl->minor_ver == SSL_MINOR_VERSION_3 )
    {
        /*
         * Only use current running hash algorithm that is already required
         * for requested ciphersuite.
         */
        ssl->handshake->verify_sig_alg = SSL_HASH_SHA256;

        if( ssl->transform_negotiate->ciphersuite_info->mac ==
            POLARSSL_MD_SHA384 )
        {
            ssl->handshake->verify_sig_alg = SSL_HASH_SHA384;
        }

        /*
         * Supported signature algorithms
         */
#if defined(POLARSSL_RSA_C)
        p[2 + sa_len++] = ssl->handshake->verify_sig_alg;
        p[2 + sa_len++] = SSL_SIG_RSA;
#endif
#if defined(POLARSSL_ECDSA_C)
        p[2 + sa_len++] = ssl->handshake->verify_sig_alg;
        p[2 + sa_len++] = SSL_SIG_ECDSA;
#endif

        p[0] = (unsigned char)( sa_len >> 8 );
        p[1] = (unsigned char)( sa_len      );
        sa_len += 2;
        p += sa_len;
    }
#endif /* POLARSSL_SSL_PROTO_TLS1_2 */

    /*
     * DistinguishedName certificate_authorities<0..2^16-1>;
     * opaque DistinguishedName<1..2^16-1>;
     */
    p += 2;
    crt = ssl->ca_chain;

    total_dn_size = 0;
    while( crt != NULL )
    {
        if( p - buf > 4096 )
            break;

        dn_size = crt->subject_raw.len;
        *p++ = (unsigned char)( dn_size >> 8 );
        *p++ = (unsigned char)( dn_size      );
        memcpy( p, crt->subject_raw.p, dn_size );
        p += dn_size;

        SSL_DEBUG_BUF( 3, "requested DN", p, dn_size );

        total_dn_size += 2 + dn_size;
        crt = crt->next;
    }

    ssl->out_msglen  = p - buf;
    ssl->out_msgtype = SSL_MSG_HANDSHAKE;
    ssl->out_msg[0]  = SSL_HS_CERTIFICATE_REQUEST;
    ssl->out_msg[4 + ct_len + sa_len] = (unsigned char)( total_dn_size  >> 8 );
    ssl->out_msg[5 + ct_len + sa_len] = (unsigned char)( total_dn_size       );

    ret = ssl_write_record( ssl );

    SSL_DEBUG_MSG( 2, ( "<= write certificate request" ) );

    return( ret );
}
#endif /* !POLARSSL_KEY_EXCHANGE_RSA_ENABLED &&
          !POLARSSL_KEY_EXCHANGE_DHE_RSA_ENABLED &&
          !POLARSSL_KEY_EXCHANGE_ECDHE_RSA_ENABLED &&
          !POLARSSL_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED */

#if defined(POLARSSL_KEY_EXCHANGE_ECDH_RSA_ENABLED) || \
    defined(POLARSSL_KEY_EXCHANGE_ECDH_ECDSA_ENABLED)
static int ssl_get_ecdh_params_from_cert( ssl_context *ssl )
{
    int ret;

    if( ! pk_can_do( ssl_own_key( ssl ), POLARSSL_PK_ECKEY ) )
    {
        SSL_DEBUG_MSG( 1, ( "server key not ECDH capable" ) );
        return( POLARSSL_ERR_SSL_PK_TYPE_MISMATCH );
    }

    if( ( ret = ecdh_get_params( &ssl->handshake->ecdh_ctx,
                                 pk_ec( *ssl_own_key( ssl ) ),
                                 POLARSSL_ECDH_OURS ) ) != 0 )
    {
        SSL_DEBUG_RET( 1, ( "ecdh_get_params" ), ret );
        return( ret );
    }

    return( 0 );
}
#endif /* POLARSSL_KEY_EXCHANGE_ECDH_RSA_ENABLED) ||
          POLARSSL_KEY_EXCHANGE_ECDH_ECDSA_ENABLED */

static int ssl_write_server_key_exchange( ssl_context *ssl )
{
    int ret;
    size_t n = 0;
    const ssl_ciphersuite_t *ciphersuite_info =
                            ssl->transform_negotiate->ciphersuite_info;

#if defined(POLARSSL_KEY_EXCHANGE_DHE_RSA_ENABLED) ||                       \
    defined(POLARSSL_KEY_EXCHANGE_DHE_PSK_ENABLED) ||                       \
    defined(POLARSSL_KEY_EXCHANGE_ECDHE_RSA_ENABLED) ||                     \
    defined(POLARSSL_KEY_EXCHANGE_ECDHE_PSK_ENABLED) ||                     \
    defined(POLARSSL_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED)
    unsigned char *p = ssl->out_msg + 4;
    unsigned char *dig_signed = p;
    size_t dig_signed_len = 0, len;
    ((void) dig_signed);
    ((void) dig_signed_len);
#endif

    SSL_DEBUG_MSG( 2, ( "=> write server key exchange" ) );

#if defined(POLARSSL_KEY_EXCHANGE_RSA_ENABLED) ||                           \
    defined(POLARSSL_KEY_EXCHANGE_PSK_ENABLED) ||                           \
    defined(POLARSSL_KEY_EXCHANGE_RSA_PSK_ENABLED)
    if( ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_RSA ||
        ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_PSK ||
        ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_RSA_PSK )
    {
        SSL_DEBUG_MSG( 2, ( "<= skip write server key exchange" ) );
        ssl->state++;
        return( 0 );
    }
#endif

#if defined(POLARSSL_KEY_EXCHANGE_ECDH_RSA_ENABLED) || \
    defined(POLARSSL_KEY_EXCHANGE_ECDH_ECDSA_ENABLED)
    if( ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_ECDH_RSA ||
        ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_ECDH_ECDSA )
    {
        ssl_get_ecdh_params_from_cert( ssl );

        SSL_DEBUG_MSG( 2, ( "<= skip write server key exchange" ) );
        ssl->state++;
        return( 0 );
    }
#endif

#if defined(POLARSSL_KEY_EXCHANGE_DHE_PSK_ENABLED) ||                       \
    defined(POLARSSL_KEY_EXCHANGE_ECDHE_PSK_ENABLED)
    if( ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_DHE_PSK ||
        ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_ECDHE_PSK )
    {
        /* TODO: Support identity hints */
        *(p++) = 0x00;
        *(p++) = 0x00;

        n += 2;
    }
#endif /* POLARSSL_KEY_EXCHANGE_DHE_PSK_ENABLED ||
          POLARSSL_KEY_EXCHANGE_ECDHE_PSK_ENABLED */

#if defined(POLARSSL_KEY_EXCHANGE_DHE_RSA_ENABLED) ||                       \
    defined(POLARSSL_KEY_EXCHANGE_DHE_PSK_ENABLED)
    if( ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_DHE_RSA ||
        ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_DHE_PSK )
    {
        /*
         * Ephemeral DH parameters:
         *
         * struct {
         *     opaque dh_p<1..2^16-1>;
         *     opaque dh_g<1..2^16-1>;
         *     opaque dh_Ys<1..2^16-1>;
         * } ServerDHParams;
         */
        if( ( ret = mpi_copy( &ssl->handshake->dhm_ctx.P, &ssl->dhm_P ) ) != 0 ||
            ( ret = mpi_copy( &ssl->handshake->dhm_ctx.G, &ssl->dhm_G ) ) != 0 )
        {
            SSL_DEBUG_RET( 1, "mpi_copy", ret );
            return( ret );
        }

        if( ( ret = dhm_make_params( &ssl->handshake->dhm_ctx,
                                      (int) mpi_size( &ssl->handshake->dhm_ctx.P ),
                                      p,
                                      &len, ssl->f_rng, ssl->p_rng ) ) != 0 )
        {
            SSL_DEBUG_RET( 1, "dhm_make_params", ret );
            return( ret );
        }

        dig_signed = p;
        dig_signed_len = len;

        p += len;
        n += len;

        SSL_DEBUG_MPI( 3, "DHM: X ", &ssl->handshake->dhm_ctx.X  );
        SSL_DEBUG_MPI( 3, "DHM: P ", &ssl->handshake->dhm_ctx.P  );
        SSL_DEBUG_MPI( 3, "DHM: G ", &ssl->handshake->dhm_ctx.G  );
        SSL_DEBUG_MPI( 3, "DHM: GX", &ssl->handshake->dhm_ctx.GX );
    }
#endif /* POLARSSL_KEY_EXCHANGE_DHE_RSA_ENABLED ||
          POLARSSL_KEY_EXCHANGE_DHE_PSK_ENABLED */

#if defined(POLARSSL_KEY_EXCHANGE__SOME__ECDHE_ENABLED)
    if( ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_ECDHE_RSA ||
        ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_ECDHE_ECDSA ||
        ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_ECDHE_PSK )
    {
        /*
         * Ephemeral ECDH parameters:
         *
         * struct {
         *     ECParameters curve_params;
         *     ECPoint      public;
         * } ServerECDHParams;
         */
        const ecp_curve_info **curve;
        curve = ssl->handshake->curves;

        if( *curve == NULL )
        {
            SSL_DEBUG_MSG( 1, ( "no matching curve for ECDHE" ) );
            return( POLARSSL_ERR_SSL_NO_CIPHER_CHOSEN );
        }

        SSL_DEBUG_MSG( 2, ( "ECDHE curve: %s", (*curve)->name ) );

        if( ( ret = ecp_use_known_dp( &ssl->handshake->ecdh_ctx.grp,
                                       (*curve)->grp_id ) ) != 0 )
        {
            SSL_DEBUG_RET( 1, "ecp_use_known_dp", ret );
            return( ret );
        }

        if( ( ret = ecdh_make_params( &ssl->handshake->ecdh_ctx, &len,
                                      p, SSL_MAX_CONTENT_LEN - n,
                                      ssl->f_rng, ssl->p_rng ) ) != 0 )
        {
            SSL_DEBUG_RET( 1, "ecdh_make_params", ret );
            return( ret );
        }

        dig_signed = p;
        dig_signed_len = len;

        p += len;
        n += len;

        SSL_DEBUG_ECP( 3, "ECDH: Q ", &ssl->handshake->ecdh_ctx.Q );
    }
#endif /* POLARSSL_KEY_EXCHANGE__SOME__ECDHE_ENABLED */

#if defined(POLARSSL_KEY_EXCHANGE_DHE_RSA_ENABLED) ||                       \
    defined(POLARSSL_KEY_EXCHANGE_ECDHE_RSA_ENABLED) ||                     \
    defined(POLARSSL_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED)
    if( ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_DHE_RSA ||
        ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_ECDHE_RSA ||
        ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_ECDHE_ECDSA )
    {
        size_t signature_len = 0;
        unsigned int hashlen = 0;
        unsigned char hash[64];
        md_type_t md_alg = POLARSSL_MD_NONE;

        /*
         * Choose hash algorithm. NONE means MD5 + SHA1 here.
         */
#if defined(POLARSSL_SSL_PROTO_TLS1_2)
        if( ssl->minor_ver == SSL_MINOR_VERSION_3 )
        {
            md_alg = ssl_md_alg_from_hash( ssl->handshake->sig_alg );

            if( md_alg == POLARSSL_MD_NONE )
            {
                SSL_DEBUG_MSG( 1, ( "should never happen" ) );
                return( POLARSSL_ERR_SSL_FEATURE_UNAVAILABLE );
            }
        }
        else
#endif
#if defined(POLARSSL_SSL_PROTO_SSL3) || defined(POLARSSL_SSL_PROTO_TLS1) || \
    defined(POLARSSL_SSL_PROTO_TLS1_1)
        if ( ciphersuite_info->key_exchange ==
                  POLARSSL_KEY_EXCHANGE_ECDHE_ECDSA )
        {
            md_alg = POLARSSL_MD_SHA1;
        }
        else
#endif
        {
            md_alg = POLARSSL_MD_NONE;
        }

        /*
         * Compute the hash to be signed
         */
#if defined(POLARSSL_SSL_PROTO_SSL3) || defined(POLARSSL_SSL_PROTO_TLS1) || \
    defined(POLARSSL_SSL_PROTO_TLS1_1)
        if( md_alg == POLARSSL_MD_NONE )
        {
            md5_context md5;
            sha1_context sha1;

            /*
             * digitally-signed struct {
             *     opaque md5_hash[16];
             *     opaque sha_hash[20];
             * };
             *
             * md5_hash
             *     MD5(ClientHello.random + ServerHello.random
             *                            + ServerParams);
             * sha_hash
             *     SHA(ClientHello.random + ServerHello.random
             *                            + ServerParams);
             */
            md5_starts( &md5 );
            md5_update( &md5, ssl->handshake->randbytes,  64 );
            md5_update( &md5, dig_signed, dig_signed_len );
            md5_finish( &md5, hash );

            sha1_starts( &sha1 );
            sha1_update( &sha1, ssl->handshake->randbytes,  64 );
            sha1_update( &sha1, dig_signed, dig_signed_len );
            sha1_finish( &sha1, hash + 16 );

            hashlen = 36;
        }
        else
#endif /* POLARSSL_SSL_PROTO_SSL3 || POLARSSL_SSL_PROTO_TLS1 || \
          POLARSSL_SSL_PROTO_TLS1_1 */
#if defined(POLARSSL_SSL_PROTO_TLS1) || defined(POLARSSL_SSL_PROTO_TLS1_1) || \
    defined(POLARSSL_SSL_PROTO_TLS1_2)
        if( md_alg != POLARSSL_MD_NONE )
        {
            md_context_t ctx;

            /* Info from md_alg will be used instead */
            hashlen = 0;

            /*
             * digitally-signed struct {
             *     opaque client_random[32];
             *     opaque server_random[32];
             *     ServerDHParams params;
             * };
             */
            if( ( ret = md_init_ctx( &ctx, md_info_from_type(md_alg) ) ) != 0 )
            {
                SSL_DEBUG_RET( 1, "md_init_ctx", ret );
                return( ret );
            }

            md_starts( &ctx );
            md_update( &ctx, ssl->handshake->randbytes, 64 );
            md_update( &ctx, dig_signed, dig_signed_len );
            md_finish( &ctx, hash );

            if( ( ret = md_free_ctx( &ctx ) ) != 0 )
            {
                SSL_DEBUG_RET( 1, "md_free_ctx", ret );
                return( ret );
            }

        }
        else
#endif /* POLARSSL_SSL_PROTO_TLS1 || POLARSSL_SSL_PROTO_TLS1_1 || \
          POLARSSL_SSL_PROTO_TLS1_2 */
        {
            SSL_DEBUG_MSG( 1, ( "should never happen" ) );
            return( POLARSSL_ERR_SSL_FEATURE_UNAVAILABLE );
        }

        SSL_DEBUG_BUF( 3, "parameters hash", hash, hashlen != 0 ? hashlen :
                (unsigned int) ( md_info_from_type( md_alg ) )->size );

        /*
         * Make the signature
         */
        if( ssl_own_key( ssl ) == NULL )
        {
            SSL_DEBUG_MSG( 1, ( "got no private key" ) );
            return( POLARSSL_ERR_SSL_PRIVATE_KEY_REQUIRED );
        }

#if defined(POLARSSL_SSL_PROTO_TLS1_2)
        if( ssl->minor_ver == SSL_MINOR_VERSION_3 )
        {
            *(p++) = ssl->handshake->sig_alg;
            *(p++) = ssl_sig_from_pk( ssl_own_key( ssl ) );

            n += 2;
        }
#endif /* POLARSSL_SSL_PROTO_TLS1_2 */

        if( ( ret = pk_sign( ssl_own_key( ssl ), md_alg, hash, hashlen,
                        p + 2 , &signature_len,
                        ssl->f_rng, ssl->p_rng ) ) != 0 )
        {
            SSL_DEBUG_RET( 1, "pk_sign", ret );
            return( ret );
        }

        *(p++) = (unsigned char)( signature_len >> 8 );
        *(p++) = (unsigned char)( signature_len      );
        n += 2;

        SSL_DEBUG_BUF( 3, "my signature", p, signature_len );

        p += signature_len;
        n += signature_len;
    }
#endif /* POLARSSL_KEY_EXCHANGE_DHE_RSA_ENABLED) ||
          POLARSSL_KEY_EXCHANGE_ECDHE_RSA_ENABLED ||
          POLARSSL_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED */

    ssl->out_msglen  = 4 + n;
    ssl->out_msgtype = SSL_MSG_HANDSHAKE;
    ssl->out_msg[0]  = SSL_HS_SERVER_KEY_EXCHANGE;

    ssl->state++;

    if( ( ret = ssl_write_record( ssl ) ) != 0 )
    {
        SSL_DEBUG_RET( 1, "ssl_write_record", ret );
        return( ret );
    }

    SSL_DEBUG_MSG( 2, ( "<= write server key exchange" ) );

    return( 0 );
}

static int ssl_write_server_hello_done( ssl_context *ssl )
{
    int ret;

    SSL_DEBUG_MSG( 2, ( "=> write server hello done" ) );

    ssl->out_msglen  = 4;
    ssl->out_msgtype = SSL_MSG_HANDSHAKE;
    ssl->out_msg[0]  = SSL_HS_SERVER_HELLO_DONE;

    ssl->state++;

    if( ( ret = ssl_write_record( ssl ) ) != 0 )
    {
        SSL_DEBUG_RET( 1, "ssl_write_record", ret );
        return( ret );
    }

    SSL_DEBUG_MSG( 2, ( "<= write server hello done" ) );

    return( 0 );
}

#if defined(POLARSSL_KEY_EXCHANGE_DHE_RSA_ENABLED) ||                       \
    defined(POLARSSL_KEY_EXCHANGE_DHE_PSK_ENABLED)
static int ssl_parse_client_dh_public( ssl_context *ssl, unsigned char **p,
                                       const unsigned char *end )
{
    int ret = POLARSSL_ERR_SSL_FEATURE_UNAVAILABLE;
    size_t n;

    /*
     * Receive G^Y mod P, premaster = (G^Y)^X mod P
     */
    if( *p + 2 > end )
    {
        SSL_DEBUG_MSG( 1, ( "bad client key exchange message" ) );
        return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_KEY_EXCHANGE );
    }

    n = ( (*p)[0] << 8 ) | (*p)[1];
    *p += 2;

    if( *p + n > end )
    {
        SSL_DEBUG_MSG( 1, ( "bad client key exchange message" ) );
        return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_KEY_EXCHANGE );
    }

    if( ( ret = dhm_read_public( &ssl->handshake->dhm_ctx, *p, n ) ) != 0 )
    {
        SSL_DEBUG_RET( 1, "dhm_read_public", ret );
        return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_KEY_EXCHANGE_RP );
    }

    *p += n;

    SSL_DEBUG_MPI( 3, "DHM: GY", &ssl->handshake->dhm_ctx.GY );

    return( ret );
}
#endif /* POLARSSL_KEY_EXCHANGE_DHE_RSA_ENABLED ||
          POLARSSL_KEY_EXCHANGE_DHE_PSK_ENABLED */

#if defined(POLARSSL_KEY_EXCHANGE_RSA_ENABLED) ||                           \
    defined(POLARSSL_KEY_EXCHANGE_RSA_PSK_ENABLED)
static int ssl_parse_encrypted_pms( ssl_context *ssl,
                                    const unsigned char *p,
                                    const unsigned char *end,
                                    size_t pms_offset )
{
    int ret;
    size_t len = pk_get_len( ssl_own_key( ssl ) );
    unsigned char *pms = ssl->handshake->premaster + pms_offset;

    if( ! pk_can_do( ssl_own_key( ssl ), POLARSSL_PK_RSA ) )
    {
        SSL_DEBUG_MSG( 1, ( "got no RSA private key" ) );
        return( POLARSSL_ERR_SSL_PRIVATE_KEY_REQUIRED );
    }

    /*
     * Decrypt the premaster using own private RSA key
     */
#if defined(POLARSSL_SSL_PROTO_TLS1) || defined(POLARSSL_SSL_PROTO_TLS1_1) || \
    defined(POLARSSL_SSL_PROTO_TLS1_2)
    if( ssl->minor_ver != SSL_MINOR_VERSION_0 )
    {
        if( *p++ != ( ( len >> 8 ) & 0xFF ) ||
            *p++ != ( ( len      ) & 0xFF ) )
        {
            SSL_DEBUG_MSG( 1, ( "bad client key exchange message" ) );
            return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_KEY_EXCHANGE );
        }
    }
#endif

    if( p + len != end )
    {
        SSL_DEBUG_MSG( 1, ( "bad client key exchange message" ) );
        return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_KEY_EXCHANGE );
    }

    ret = pk_decrypt( ssl_own_key( ssl ), p, len,
                      pms, &ssl->handshake->pmslen,
                      sizeof( ssl->handshake->premaster ) - pms_offset,
                      ssl->f_rng, ssl->p_rng );

    if( ret != 0 || ssl->handshake->pmslen != 48 ||
        pms[0] != ssl->handshake->max_major_ver ||
        pms[1] != ssl->handshake->max_minor_ver )
    {
        SSL_DEBUG_MSG( 1, ( "bad client key exchange message" ) );

        /*
         * Protection against Bleichenbacher's attack:
         * invalid PKCS#1 v1.5 padding must not cause
         * the connection to end immediately; instead,
         * send a bad_record_mac later in the handshake.
         */
        ssl->handshake->pmslen = 48;

        ret = ssl->f_rng( ssl->p_rng, pms, ssl->handshake->pmslen );
        if( ret != 0 )
            return( ret );
    }

    return( ret );
}
#endif /* POLARSSL_KEY_EXCHANGE_RSA_ENABLED ||
          POLARSSL_KEY_EXCHANGE_RSA_PSK_ENABLED */

#if defined(POLARSSL_KEY_EXCHANGE__SOME__PSK_ENABLED)
static int ssl_parse_client_psk_identity( ssl_context *ssl, unsigned char **p,
                                          const unsigned char *end )
{
    int ret = 0;
    size_t n;

    if( ssl->f_psk == NULL &&
        ( ssl->psk == NULL || ssl->psk_identity == NULL ||
          ssl->psk_identity_len == 0 || ssl->psk_len == 0 ) )
    {
        SSL_DEBUG_MSG( 1, ( "got no pre-shared key" ) );
        return( POLARSSL_ERR_SSL_PRIVATE_KEY_REQUIRED );
    }

    /*
     * Receive client pre-shared key identity name
     */
    if( *p + 2 > end )
    {
        SSL_DEBUG_MSG( 1, ( "bad client key exchange message" ) );
        return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_KEY_EXCHANGE );
    }

    n = ( (*p)[0] << 8 ) | (*p)[1];
    *p += 2;

    if( n < 1 || n > 65535 || *p + n > end )
    {
        SSL_DEBUG_MSG( 1, ( "bad client key exchange message" ) );
        return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_KEY_EXCHANGE );
    }

    if( ssl->f_psk != NULL )
    {
        if( ( ret != ssl->f_psk( ssl->p_psk, ssl, *p, n ) ) != 0 )
            ret = POLARSSL_ERR_SSL_UNKNOWN_IDENTITY;
    }

    if( ret == 0 )
    {
        /* Identity is not a big secret since clients send it in the clear,
         * but treat it carefully anyway, just in case */
        if( n != ssl->psk_identity_len ||
            safer_memcmp( ssl->psk_identity, *p, n ) != 0 )
        {
            ret = POLARSSL_ERR_SSL_UNKNOWN_IDENTITY;
        }
    }

    if( ret == POLARSSL_ERR_SSL_UNKNOWN_IDENTITY )
    {
        SSL_DEBUG_BUF( 3, "Unknown PSK identity", *p, n );
        if( ( ret = ssl_send_alert_message( ssl,
                              SSL_ALERT_LEVEL_FATAL,
                              SSL_ALERT_MSG_UNKNOWN_PSK_IDENTITY ) ) != 0 )
        {
            return( ret );
        }

        return( POLARSSL_ERR_SSL_UNKNOWN_IDENTITY );
    }

    *p += n;
    ret = 0;

    return( ret );
}
#endif /* POLARSSL_KEY_EXCHANGE__SOME__PSK_ENABLED */

static int ssl_parse_client_key_exchange( ssl_context *ssl )
{
    int ret;
    const ssl_ciphersuite_t *ciphersuite_info;

    ciphersuite_info = ssl->transform_negotiate->ciphersuite_info;

    SSL_DEBUG_MSG( 2, ( "=> parse client key exchange" ) );

    if( ( ret = ssl_read_record( ssl ) ) != 0 )
    {
        SSL_DEBUG_RET( 1, "ssl_read_record", ret );
        return( ret );
    }

    if( ssl->in_msgtype != SSL_MSG_HANDSHAKE )
    {
        SSL_DEBUG_MSG( 1, ( "bad client key exchange message" ) );
        return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_KEY_EXCHANGE );
    }

    if( ssl->in_msg[0] != SSL_HS_CLIENT_KEY_EXCHANGE )
    {
        SSL_DEBUG_MSG( 1, ( "bad client key exchange message" ) );
        return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_KEY_EXCHANGE );
    }

#if defined(POLARSSL_KEY_EXCHANGE_DHE_RSA_ENABLED)
    if( ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_DHE_RSA )
    {
        unsigned char *p = ssl->in_msg + 4;
        unsigned char *end = ssl->in_msg + ssl->in_hslen;

        if( ( ret = ssl_parse_client_dh_public( ssl, &p, end ) ) != 0 )
        {
            SSL_DEBUG_RET( 1, ( "ssl_parse_client_dh_public" ), ret );
            return( ret );
        }

        if( p != end )
        {
            SSL_DEBUG_MSG( 1, ( "bad client key exchange" ) );
            return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_KEY_EXCHANGE );
        }

        ssl->handshake->pmslen = ssl->handshake->dhm_ctx.len;

        if( ( ret = dhm_calc_secret( &ssl->handshake->dhm_ctx,
                                      ssl->handshake->premaster,
                                     &ssl->handshake->pmslen,
                                      ssl->f_rng, ssl->p_rng ) ) != 0 )
        {
            SSL_DEBUG_RET( 1, "dhm_calc_secret", ret );
            return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_KEY_EXCHANGE_CS );
        }

        SSL_DEBUG_MPI( 3, "DHM: K ", &ssl->handshake->dhm_ctx.K  );
    }
    else
#endif /* POLARSSL_KEY_EXCHANGE_DHE_RSA_ENABLED */
#if defined(POLARSSL_KEY_EXCHANGE_ECDHE_RSA_ENABLED) ||                     \
    defined(POLARSSL_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED) ||                   \
    defined(POLARSSL_KEY_EXCHANGE_ECDH_RSA_ENABLED) ||                      \
    defined(POLARSSL_KEY_EXCHANGE_ECDH_ECDSA_ENABLED)
    if( ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_ECDHE_RSA ||
        ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_ECDHE_ECDSA ||
        ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_ECDH_RSA ||
        ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_ECDH_ECDSA )
    {
        if( ( ret = ecdh_read_public( &ssl->handshake->ecdh_ctx,
                                       ssl->in_msg + 4, ssl->in_hslen - 4 ) ) != 0 )
        {
            SSL_DEBUG_RET( 1, "ecdh_read_public", ret );
            return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_KEY_EXCHANGE_RP );
        }

        SSL_DEBUG_ECP( 3, "ECDH: Qp ", &ssl->handshake->ecdh_ctx.Qp );

        if( ( ret = ecdh_calc_secret( &ssl->handshake->ecdh_ctx,
                                      &ssl->handshake->pmslen,
                                       ssl->handshake->premaster,
                                       POLARSSL_MPI_MAX_SIZE,
                                       ssl->f_rng, ssl->p_rng ) ) != 0 )
        {
            SSL_DEBUG_RET( 1, "ecdh_calc_secret", ret );
            return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_KEY_EXCHANGE_CS );
        }

        SSL_DEBUG_MPI( 3, "ECDH: z  ", &ssl->handshake->ecdh_ctx.z );
    }
    else
#endif /* POLARSSL_KEY_EXCHANGE_ECDHE_RSA_ENABLED ||
          POLARSSL_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED ||
          POLARSSL_KEY_EXCHANGE_ECDH_RSA_ENABLED ||
          POLARSSL_KEY_EXCHANGE_ECDH_ECDSA_ENABLED */
#if defined(POLARSSL_KEY_EXCHANGE_PSK_ENABLED)
    if( ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_PSK )
    {
        unsigned char *p = ssl->in_msg + 4;
        unsigned char *end = ssl->in_msg + ssl->in_hslen;

        if( ( ret = ssl_parse_client_psk_identity( ssl, &p, end ) ) != 0 )
        {
            SSL_DEBUG_RET( 1, ( "ssl_parse_client_psk_identity" ), ret );
            return( ret );
        }

        if( p != end )
        {
            SSL_DEBUG_MSG( 1, ( "bad client key exchange" ) );
            return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_KEY_EXCHANGE );
        }

        if( ( ret = ssl_psk_derive_premaster( ssl,
                        ciphersuite_info->key_exchange ) ) != 0 )
        {
            SSL_DEBUG_RET( 1, "ssl_psk_derive_premaster", ret );
            return( ret );
        }
    }
    else
#endif /* POLARSSL_KEY_EXCHANGE_PSK_ENABLED */
#if defined(POLARSSL_KEY_EXCHANGE_RSA_PSK_ENABLED)
    if( ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_RSA_PSK )
    {
        unsigned char *p = ssl->in_msg + 4;
        unsigned char *end = ssl->in_msg + ssl->in_hslen;

        if( ( ret = ssl_parse_client_psk_identity( ssl, &p, end ) ) != 0 )
        {
            SSL_DEBUG_RET( 1, ( "ssl_parse_client_psk_identity" ), ret );
            return( ret );
        }

        if( ( ret = ssl_parse_encrypted_pms( ssl, p, end, 2 ) ) != 0 )
        {
            SSL_DEBUG_RET( 1, ( "ssl_parse_encrypted_pms" ), ret );
            return( ret );
        }

        if( ( ret = ssl_psk_derive_premaster( ssl,
                        ciphersuite_info->key_exchange ) ) != 0 )
        {
            SSL_DEBUG_RET( 1, "ssl_psk_derive_premaster", ret );
            return( ret );
        }
    }
    else
#endif /* POLARSSL_KEY_EXCHANGE_RSA_PSK_ENABLED */
#if defined(POLARSSL_KEY_EXCHANGE_DHE_PSK_ENABLED)
    if( ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_DHE_PSK )
    {
        unsigned char *p = ssl->in_msg + 4;
        unsigned char *end = ssl->in_msg + ssl->in_hslen;

        if( ( ret = ssl_parse_client_psk_identity( ssl, &p, end ) ) != 0 )
        {
            SSL_DEBUG_RET( 1, ( "ssl_parse_client_psk_identity" ), ret );
            return( ret );
        }
        if( ( ret = ssl_parse_client_dh_public( ssl, &p, end ) ) != 0 )
        {
            SSL_DEBUG_RET( 1, ( "ssl_parse_client_dh_public" ), ret );
            return( ret );
        }

        if( p != end )
        {
            SSL_DEBUG_MSG( 1, ( "bad client key exchange" ) );
            return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_KEY_EXCHANGE );
        }

        if( ( ret = ssl_psk_derive_premaster( ssl,
                        ciphersuite_info->key_exchange ) ) != 0 )
        {
            SSL_DEBUG_RET( 1, "ssl_psk_derive_premaster", ret );
            return( ret );
        }
    }
    else
#endif /* POLARSSL_KEY_EXCHANGE_DHE_PSK_ENABLED */
#if defined(POLARSSL_KEY_EXCHANGE_ECDHE_PSK_ENABLED)
    if( ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_ECDHE_PSK )
    {
        unsigned char *p = ssl->in_msg + 4;
        unsigned char *end = ssl->in_msg + ssl->in_hslen;

        if( ( ret = ssl_parse_client_psk_identity( ssl, &p, end ) ) != 0 )
        {
            SSL_DEBUG_RET( 1, ( "ssl_parse_client_psk_identity" ), ret );
            return( ret );
        }

        if( ( ret = ecdh_read_public( &ssl->handshake->ecdh_ctx,
                                       p, end - p ) ) != 0 )
        {
            SSL_DEBUG_RET( 1, "ecdh_read_public", ret );
            return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_KEY_EXCHANGE_RP );
        }

        SSL_DEBUG_ECP( 3, "ECDH: Qp ", &ssl->handshake->ecdh_ctx.Qp );

        if( ( ret = ssl_psk_derive_premaster( ssl,
                        ciphersuite_info->key_exchange ) ) != 0 )
        {
            SSL_DEBUG_RET( 1, "ssl_psk_derive_premaster", ret );
            return( ret );
        }
    }
    else
#endif /* POLARSSL_KEY_EXCHANGE_ECDHE_PSK_ENABLED */
#if defined(POLARSSL_KEY_EXCHANGE_RSA_ENABLED)
    if( ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_RSA )
    {
        if( ( ret = ssl_parse_encrypted_pms( ssl,
                                             ssl->in_msg + 4,
                                             ssl->in_msg + ssl->in_hslen,
                                             0 ) ) != 0 )
        {
            SSL_DEBUG_RET( 1, ( "ssl_parse_parse_encrypted_pms_secret" ), ret );
            return( ret );
        }
    }
    else
#endif /* POLARSSL_KEY_EXCHANGE_RSA_ENABLED */
    {
        SSL_DEBUG_MSG( 1, ( "should never happen" ) );
        return( POLARSSL_ERR_SSL_FEATURE_UNAVAILABLE );
    }

    if( ( ret = ssl_derive_keys( ssl ) ) != 0 )
    {
        SSL_DEBUG_RET( 1, "ssl_derive_keys", ret );
        return( ret );
    }

    ssl->state++;

    SSL_DEBUG_MSG( 2, ( "<= parse client key exchange" ) );

    return( 0 );
}

#if !defined(POLARSSL_KEY_EXCHANGE_RSA_ENABLED)       && \
    !defined(POLARSSL_KEY_EXCHANGE_DHE_RSA_ENABLED)   && \
    !defined(POLARSSL_KEY_EXCHANGE_ECDHE_RSA_ENABLED) && \
    !defined(POLARSSL_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED)
static int ssl_parse_certificate_verify( ssl_context *ssl )
{
    int ret = POLARSSL_ERR_SSL_FEATURE_UNAVAILABLE;
    const ssl_ciphersuite_t *ciphersuite_info = ssl->transform_negotiate->ciphersuite_info;

    SSL_DEBUG_MSG( 2, ( "=> parse certificate verify" ) );

    if( ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_PSK ||
        ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_RSA_PSK ||
        ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_ECDHE_PSK ||
        ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_DHE_PSK )
    {
        SSL_DEBUG_MSG( 2, ( "<= skip parse certificate verify" ) );
        ssl->state++;
        return( 0 );
    }

    SSL_DEBUG_MSG( 1, ( "should not happen" ) );
    return( ret );
}
#else
static int ssl_parse_certificate_verify( ssl_context *ssl )
{
    int ret = POLARSSL_ERR_SSL_FEATURE_UNAVAILABLE;
    size_t sa_len, sig_len;
    unsigned char hash[48];
    unsigned char *hash_start = hash;
    size_t hashlen;
#if defined(POLARSSL_SSL_PROTO_TLS1_2)
    pk_type_t pk_alg;
#endif
    md_type_t md_alg;
    const ssl_ciphersuite_t *ciphersuite_info = ssl->transform_negotiate->ciphersuite_info;

    SSL_DEBUG_MSG( 2, ( "=> parse certificate verify" ) );

    if( ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_PSK ||
        ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_RSA_PSK ||
        ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_ECDHE_PSK ||
        ciphersuite_info->key_exchange == POLARSSL_KEY_EXCHANGE_DHE_PSK )
    {
        SSL_DEBUG_MSG( 2, ( "<= skip parse certificate verify" ) );
        ssl->state++;
        return( 0 );
    }

    if( ssl->session_negotiate->peer_cert == NULL )
    {
        SSL_DEBUG_MSG( 2, ( "<= skip parse certificate verify" ) );
        ssl->state++;
        return( 0 );
    }

    ssl->handshake->calc_verify( ssl, hash );

    if( ( ret = ssl_read_record( ssl ) ) != 0 )
    {
        SSL_DEBUG_RET( 1, "ssl_read_record", ret );
        return( ret );
    }

    ssl->state++;

    if( ssl->in_msgtype != SSL_MSG_HANDSHAKE )
    {
        SSL_DEBUG_MSG( 1, ( "bad certificate verify message" ) );
        return( POLARSSL_ERR_SSL_BAD_HS_CERTIFICATE_VERIFY );
    }

    if( ssl->in_msg[0] != SSL_HS_CERTIFICATE_VERIFY )
    {
        SSL_DEBUG_MSG( 1, ( "bad certificate verify message" ) );
        return( POLARSSL_ERR_SSL_BAD_HS_CERTIFICATE_VERIFY );
    }

    /*
     *     0  .   0   handshake type
     *     1  .   3   handshake length
     *     4  .   5   sig alg (TLS 1.2 only)
     *    4+n .  5+n  signature length (n = sa_len)
     *    6+n . 6+n+m signature (m = sig_len)
     */

#if defined(POLARSSL_SSL_PROTO_SSL3) || defined(POLARSSL_SSL_PROTO_TLS1) || \
    defined(POLARSSL_SSL_PROTO_TLS1_1)
    if( ssl->minor_ver != SSL_MINOR_VERSION_3 )
    {
        sa_len = 0;

        md_alg = POLARSSL_MD_NONE;
        hashlen = 36;

        /* For ECDSA, use SHA-1, not MD-5 + SHA-1 */
        if( pk_can_do( &ssl->session_negotiate->peer_cert->pk,
                        POLARSSL_PK_ECDSA ) )
        {
            hash_start += 16;
            hashlen -= 16;
            md_alg = POLARSSL_MD_SHA1;
        }
    }
    else
#endif
#if defined(POLARSSL_SSL_PROTO_TLS1_2)
    if( ssl->minor_ver == SSL_MINOR_VERSION_3 )
    {
        sa_len = 2;

        /*
         * Hash
         */
        if( ssl->in_msg[4] != ssl->handshake->verify_sig_alg )
        {
            SSL_DEBUG_MSG( 1, ( "peer not adhering to requested sig_alg"
                                " for verify message" ) );
            return( POLARSSL_ERR_SSL_BAD_HS_CERTIFICATE_VERIFY );
        }

        md_alg = ssl_md_alg_from_hash( ssl->handshake->verify_sig_alg );

        /* Info from md_alg will be used instead */
        hashlen = 0;

        /*
         * Signature
         */
        if( ( pk_alg = ssl_pk_alg_from_sig( ssl->in_msg[5] ) )
                        == POLARSSL_PK_NONE )
        {
            SSL_DEBUG_MSG( 1, ( "peer not adhering to requested sig_alg"
                                " for verify message" ) );
            return( POLARSSL_ERR_SSL_BAD_HS_CERTIFICATE_VERIFY );
        }

        /*
         * Check the certificate's key type matches the signature alg
         */
        if( ! pk_can_do( &ssl->session_negotiate->peer_cert->pk, pk_alg ) )
        {
            SSL_DEBUG_MSG( 1, ( "sig_alg doesn't match cert key" ) );
            return( POLARSSL_ERR_SSL_BAD_HS_CERTIFICATE_VERIFY );
        }
    }
    else
#endif /* POLARSSL_SSL_PROTO_TLS1_2 */
    {
        SSL_DEBUG_MSG( 1, ( "should never happen" ) );
        return( POLARSSL_ERR_SSL_FEATURE_UNAVAILABLE );
    }

    sig_len = ( ssl->in_msg[4 + sa_len] << 8 ) | ssl->in_msg[5 + sa_len];

    if( sa_len + sig_len + 6 != ssl->in_hslen )
    {
        SSL_DEBUG_MSG( 1, ( "bad certificate verify message" ) );
        return( POLARSSL_ERR_SSL_BAD_HS_CERTIFICATE_VERIFY );
    }

    if( ( ret = pk_verify( &ssl->session_negotiate->peer_cert->pk,
                           md_alg, hash_start, hashlen,
                           ssl->in_msg + 6 + sa_len, sig_len ) ) != 0 )
    {
        SSL_DEBUG_RET( 1, "pk_verify", ret );
        return( ret );
    }

    SSL_DEBUG_MSG( 2, ( "<= parse certificate verify" ) );

    return( ret );
}
#endif /* !POLARSSL_KEY_EXCHANGE_RSA_ENABLED &&
          !POLARSSL_KEY_EXCHANGE_DHE_RSA_ENABLED &&
          !POLARSSL_KEY_EXCHANGE_ECDHE_RSA_ENABLED */

		  /*
 * SSL handshake -- server side -- single step
 */
int ssl_handshake_server_step( ssl_context *ssl )
{
    int ret = 0;

    if( ssl->state == SSL_HANDSHAKE_OVER )
        return( POLARSSL_ERR_SSL_BAD_INPUT_DATA );

    SSL_DEBUG_MSG( 2, ( "server state: %d", ssl->state ) );

    if( ( ret = ssl_flush_output( ssl ) ) != 0 )
        return( ret );

    switch( ssl->state )
    {
        case SSL_HELLO_REQUEST:
            ssl->state = SSL_CLIENT_HELLO;
            break;

        /*
         *  <==   ClientHello
         */
        case SSL_CLIENT_HELLO:
            ret = ssl_parse_client_hello( ssl );
            break;

        /*
         *  ==>   ServerHello
         *        Certificate
         *      ( ServerKeyExchange  )
         *      ( CertificateRequest )
         *        ServerHelloDone
         */
        case SSL_SERVER_HELLO:
            ret = ssl_write_server_hello( ssl );
            break;

        case SSL_SERVER_CERTIFICATE:
            ret = ssl_write_certificate( ssl );
            break;

        case SSL_SERVER_KEY_EXCHANGE:
            ret = ssl_write_server_key_exchange( ssl );
            break;

        case SSL_CERTIFICATE_REQUEST:
            ret = ssl_write_certificate_request( ssl );
            break;

        case SSL_SERVER_HELLO_DONE:
            ret = ssl_write_server_hello_done( ssl );
            break;

        /*
         *  <== ( Certificate/Alert  )
         *        ClientKeyExchange
         *      ( CertificateVerify  )
         *        ChangeCipherSpec
         *        Finished
         */
        case SSL_CLIENT_CERTIFICATE:
            ret = ssl_parse_certificate( ssl );
            break;

        case SSL_CLIENT_KEY_EXCHANGE:
            ret = ssl_parse_client_key_exchange( ssl );
            break;

        case SSL_CERTIFICATE_VERIFY:
            ret = ssl_parse_certificate_verify( ssl );
            break;

        case SSL_CLIENT_CHANGE_CIPHER_SPEC:
            ret = ssl_parse_change_cipher_spec( ssl );
            break;

        case SSL_CLIENT_FINISHED:
            ret = ssl_parse_finished( ssl );
            break;

        /*
         *  ==> ( NewSessionTicket )
         *        ChangeCipherSpec
         *        Finished
         */
        case SSL_SERVER_CHANGE_CIPHER_SPEC:
                ret = ssl_write_change_cipher_spec( ssl );
            break;

        case SSL_SERVER_FINISHED:
            ret = ssl_write_finished( ssl );
            break;

        case SSL_FLUSH_BUFFERS:
            SSL_DEBUG_MSG( 2, ( "handshake: done" ) );
            ssl->state = SSL_HANDSHAKE_WRAPUP;
            break;

        case SSL_HANDSHAKE_WRAPUP:
            ssl_handshake_wrapup( ssl );
            break;

        default:
            SSL_DEBUG_MSG( 1, ( "invalid state %d", ssl->state ) );
            return( POLARSSL_ERR_SSL_BAD_INPUT_DATA );
    }

    return( ret );
}
#endif
