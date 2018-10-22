/*
 * Copyright (c) 2012-2017, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * */
/*
 * ======== inetaddr.c ========
 *
 * Standard address conversion functions
 *
 */

#include <stdlib.h>
#include <ctype.h>
#include <netmain.h>
#include <stkmain.h>
#include <_stack.h>

/* Size needed for getaddrinfo mem allocation */
#define NDK_ADDRINFO_ALLOCSZ ((sizeof(struct addrinfo)) + \
                                 (sizeof(struct sockaddr_in6)))

/* Cap number of mem allocations in case of large number of results from DNS */
#define NDK_ADDRINFO_MAX_DNS_NODES 10

#define NDK_DNSBUFSIZE 512

static struct addrinfo *setAddrInfo(struct sockaddr *addr, int family,
        const char *service, const struct addrinfo *hints);

static struct addrinfo *createAddrInfo(struct sockaddr *addr, int family,
        const char *service, int flags, int socktype, int protocol);

static int mergeLists(struct addrinfo **curList, struct addrinfo **newList);

/*
 * inet_addr()
 * Converts string to network format IP address
 */
uint32_t inet_addr( const char *str )
{
    struct in_addr result;

    if( inet_aton(str, &result) )
        return( result.s_addr );
    return( 0 );
}

/* 
 * inet_aton()
 * Convert string to network address structure
 */
int inet_aton( const char *str, struct in_addr *addr)
{
    uint32_t Val[4];
    uint32_t Base;
    int    Sect;
    char   c;

    Sect = -1;
    while( *str )
    {
        /* New section */
        Sect++;

        /* Get the base for this number */
        Base = 10;
        if (*str == '0')
        {
            if( *(str+1) == 'x' || *(str+1) == 'X' )
            {
                Base = 16;
                str += 2;
            }
            else
            {
                Base = 8;
                str++;
            }
        }

        /* Now decode this number */
        Val[Sect] = 0;
        for(;;)
        {
            c = *str++;

            if( (c >= '0' && c <= '9') )
                Val[Sect] = (Val[Sect]*Base) + (c-'0');
            else if( Base == 16 && (c >= 'A' && c <= 'F') )
                Val[Sect] = (Val[Sect]*16) + (c-'A') + 10;
            else if( Base == 16 && (c >= 'a' && c <= 'f') )
                Val[Sect] = (Val[Sect]*16) + (c-'a') + 10;
            else if( c == '.' )
            {
                /* Validate value */
                if( Val[Sect] > 255 )
                    return(0);

                /* Once we have four sections, quit */
                /* We want to accept: "1.2.3.4.in-addr.arpa" */
                if( Sect == 3 )
                    goto done;

                /* Break this section */
                break;
            }
            else if( !c )
                goto done;
            else if( c != ' ' )
                return(0);
        }
    }

done:
    /* What we do changes based on the number of sections */
    switch( Sect )
    {
    default:
        return(0);
    case 0:
        addr->s_addr = Val[0];
        break;
    case 1:
        if( Val[1] > 0xffffff )
            return(0);
        addr->s_addr = Val[0]<<24;
        addr->s_addr += Val[1];
        break;
    case 2:
        if( Val[2] > 0xffff )
            return(0);
        addr->s_addr = Val[0]<<24;
        addr->s_addr += (Val[1]<<16);
        addr->s_addr += Val[2];
        break;
    case 3:
        if( Val[3] > 0xff )
            return(0);
        addr->s_addr = Val[0]<<24;
        addr->s_addr += (Val[1]<<16);
        addr->s_addr += (Val[2]<<8);
        addr->s_addr += Val[3];
        break;
    }

    addr->s_addr = NDK_htonl(addr->s_addr);
    return(1);
}

/*
 * inet_ntop()
 * Standard means of converting an IP address to a string.
 */
const char *inet_ntop(int af, const void *src, char *dst, int cnt)
{
    if (!src || !dst || (cnt == 0)) {
        DbgPrintf(DBG_WARN, "inet_ntop: error: invalid arguments passed\n");
        /* invalid args passed */
        return (NULL);
    }
    else if (af == AF_INET) {
        NtIPN2Str(*((uint32_t *)src), dst);
    }
#ifdef _INCLUDE_IPv6_CODE
    else if (af == AF_INET6) {
        IPv6IPAddressToString(*((IP6N *)src), dst);
    }
#endif
    else {
        DbgPrintf(DBG_WARN, "inet_ntop: error: invalid family\n");
        return (NULL);
    }

    return (dst);
}

/*
 * inet_pton()
 * Standard means of converting a string to network address structure.
 */
int inet_pton(int af, const char *src, void *dst)
{
    int status;

    if (!src || !dst) {
        /* invalid args passed */
        DbgPrintf(DBG_WARN, "inet_pton: error: invalid arguments passed\n");
        status = -1;
    }
    else if (af == AF_INET) {
        status = inet_aton(src, dst);
    }
#ifdef _INCLUDE_IPv6_CODE
    else if (af == AF_INET6) {
        status = (IPv6StringToIPAddress((char *)src, dst) == 0);
    }
#endif
    else {
        DbgPrintf(DBG_WARN, "inet_pton: error: invalid family\n");
        status = -1;
    }

    return (status);
}

/*
 * getaddrinfo()
 * Create a IPv4 or IPv6 socket address structure, to be used with bind()
 * and/or connect() to create a client or server socket
 *
 * This is a "minimal" version for initial BSD support in NDK.  Supports a host
 * name or an IPv4 or IPv6 address string passed in via the 'node' parameter
 * for creating a client socket.  A value of NULL should be passed for 'node'
 * with AI_PASSIVE flag set to create a (non-loopback) server socket.
 */
int getaddrinfo(const char *node, const char *service,
        const struct addrinfo *hints, struct addrinfo **res)
{
    int retval = 0;
    int i;
    int resolved = 0;
    int numNodes = 0;
    HOSTENT *dnsInfo = NULL;
    char *buffer = NULL;
    struct addrinfo *ai = NULL;
    struct addrinfo *aiTmp = NULL;
    struct sockaddr_in sin;
#ifdef _INCLUDE_IPv6_CODE
    struct sockaddr_in6 sin6;
#endif

    /* check args passed in for errors */
    if (!node && !service) {
        DbgPrintf(DBG_WARN,
            "getaddrinfo: Error: node and service args cannot both be NULL\n");
        return (EAI_NONAME);
    }

    if (!service) {
        service = "0";
    }

    if (!res) {
        DbgPrintf(DBG_WARN,
                "getaddrinfo: Error: res cannot be NULL\n");
        return (EAI_NONAME);
    }

    if (!hints) {
        /* User passed NULL hints. Create a hints for them with all 0 values */
        static const struct addrinfo defHints = {
            0,         /* ai_flags */
            AF_UNSPEC, /* ai_family */
            0,         /* ai_socktype */
            0,         /* ai_protocol */
            0,         /* ai_addrlen */
            NULL,      /* ai_addr */
            NULL,      /* ai_canonname */
            NULL       /* ai_next */
        };
        hints = &defHints;
    }
    else {
        /* Check the user's hints for invalid settings */
        if (hints->ai_socktype != SOCK_STREAM &&
                hints->ai_socktype != SOCK_DGRAM &&
                hints->ai_socktype != 0) {
            DbgPrintf(DBG_WARN,
                    "getaddrinfo: error: invalid or unknown socktype\n");
            return (EAI_SOCKTYPE);
        }
        else if (hints->ai_protocol != IPPROTO_TCP &&
                hints->ai_protocol != IPPROTO_UDP &&
                hints->ai_protocol != 0) {
            DbgPrintf(DBG_WARN,
                    "getaddrinfo: error: invalid or unknown protocol\n");
            return (EAI_SOCKTYPE);
        }
        else if ((hints->ai_family != AF_INET) &&
                (hints->ai_family != AF_INET6) &&
                (hints->ai_family != AF_UNSPEC)) {
            DbgPrintf(DBG_WARN,
                    "getaddrinfo: error: invalid or unknown family\n");
            return (EAI_FAMILY);
        }
    }

    /* initialize variables */
    memset(&sin, 0, sizeof(struct sockaddr_in));
#ifdef _INCLUDE_IPv6_CODE
    memset(&sin6, 0, sizeof(struct sockaddr_in6));
#endif

    if (node) {
        /*
         * Client case. User needs an address structure to call connect() with.
         *
         * Determine what caller has passed to us for 'node'. Should be either:
         *     - an IPv4 address
         *     - an IPv6 address
         *     - or a hostname
         */

        /* Test if 'node' is an IPv4 address */
        retval = inet_aton(node, &(sin.sin_addr));
        if (retval) {
            /* Ensure address family matches */
            if (hints->ai_family != AF_INET &&
                    hints->ai_family != AF_UNSPEC) {
                return (EAI_ADDRFAMILY);
            }

            /*
             * Create addrinfo struct(s) containing this IPv4 address. If ai
             * is NULL, this will be caught at end of getaddrinfo
             */
            ai = setAddrInfo((struct sockaddr *)&sin, AF_INET, service,
                    hints);
        }
        else {
            /* 'node' is either an IPv6 address or a hostname (or invalid) */
#ifdef _INCLUDE_IPv6_CODE
            /* Test if 'node' is an IPv6 address */
            retval = inet_pton(AF_INET6, node, &(sin6.sin6_addr));
            if (retval > 0) {
                /* Ensure address family matches */
                if (hints->ai_family != AF_INET6 &&
                        hints->ai_family != AF_UNSPEC) {
                    return (EAI_ADDRFAMILY);
                }

                /*
                 * Create addrinfo struct(s) containing this IPv6 address. If ai
                 * is NULL, this will be caught at end of getaddrinfo
                 */
                ai = setAddrInfo((struct sockaddr *)&sin6, AF_INET6, service,
                    hints);
            }
            else {
#endif
                /*
                 * Per RFC 2553, if node is not a valid numeric address string
                 * and AI_NUMERICHOST is set, return error (and prevent call to
                 * DNS).
                 */
                if (hints->ai_flags & AI_NUMERICHOST) {
                    return (EAI_NONAME);
                }

                /* Test if 'node' a host name. Use DNS to resolve it. */
                buffer = mmAlloc(NDK_DNSBUFSIZE);
                if (!buffer) {
                    DbgPrintf(DBG_WARN,
                           "getaddrinfo: Error: couldn't alloc DNS buffer\n");
                    return (EAI_MEMORY);
                }

                dnsInfo = (HOSTENT *)buffer;

                /* IPv4 DNS lookup */
                if (hints->ai_family == AF_INET ||
                        hints->ai_family == AF_UNSPEC) {
#ifdef _INCLUDE_IPv6_CODE
                    retval = DNSGetHostByName2((char *)node, AF_INET, buffer,
                            NDK_DNSBUFSIZE);
#else
                    retval = DNSGetHostByName((char *)node, buffer,
                            NDK_DNSBUFSIZE);
#endif
                    if (retval == 0) {
                        resolved = 1;
                        for (i = 0; i < dnsInfo->h_addrcnt &&
                                numNodes < NDK_ADDRINFO_MAX_DNS_NODES; i++) {
#ifdef _INCLUDE_IPv6_CODE
                            sin.sin_addr.s_addr =
                                    (uint32_t)RdNet32(dnsInfo->h_addr_list[i]);
#else
                            sin.sin_addr.s_addr = dnsInfo->h_addr[i];
#endif
                            /*
                             * Create addrinfo struct(s) containing this IPv4
                             * address. This can return a list with multiple
                             * nodes, depending on hints provided. Empty lists
                             * are handled before returning.
                             */
                            aiTmp = setAddrInfo((struct sockaddr *)&sin,
                                    AF_INET, service, hints);

                            if (aiTmp) {
                                /*
                                 * Merge the results into the main list
                                 * for each loop iteration:
                                 */
                                numNodes += mergeLists(&ai, &aiTmp);
                            }
                        }
                    }
                }

#ifdef _INCLUDE_IPv6_CODE
                /* IPv6 DNS lookup */
                if (hints->ai_family == AF_INET6 ||
                        hints->ai_family == AF_UNSPEC) {

                    retval = DNSGetHostByName2((char *)node, AF_INET6, buffer,
                            NDK_DNSBUFSIZE);

                    if (retval == 0) {
                        /* Workaround for CQ114965 (IPv6 DNS false positive) */
                        if (dnsInfo->h_addrcnt == 0) {
                            /* Skip loop and return error */
                            resolved = 0;
                        }
                        else {
                            resolved = 1;
                        }

                        for (i = 0; i < dnsInfo->h_addrcnt &&
                                numNodes < NDK_ADDRINFO_MAX_DNS_NODES; i++) {
                            /*
                             * Create addrinfo struct containing this IPv6
                             * address
                             */
                            memcpy(&(sin6.sin6_addr),
                                    (struct in6_addr *)dnsInfo->h_addr_list[i],
                                    sizeof(struct in6_addr));

                            /*
                             * Create addrinfo struct(s) containing this IPv6
                             * address. This can return a list with multiple
                             * nodes, depending on hints provided. Empty lists
                             * are handled before returning.
                             */
                            aiTmp = setAddrInfo((struct sockaddr *)&sin6,
                                    AF_INET6, service, hints);

                            if (aiTmp) {
                                /*
                                 * Merge the results into the main list
                                 * for each loop iteration:
                                 */
                                numNodes += mergeLists(&ai, &aiTmp);
                            }
                        }
                    }
                }
#endif
                mmFree(buffer);

                if (!resolved) {
                    DbgPrintf(DBG_WARN,
                      "getaddrinfo: Error: couldn't resolve host name \"%s\"\n",
                      node);
                    return (EAI_NONAME);
                }
#ifdef _INCLUDE_IPv6_CODE
            }
#endif
        }
    }
    else {
        /* Server case. User needs an address structure to call bind() with. */
        if (hints->ai_family == AF_INET || hints->ai_family == AF_UNSPEC) {
            if (hints->ai_flags & AI_PASSIVE) {
                /* Per RFC 2553, accept connections on any IF */
                sin.sin_addr.s_addr = NDK_htonl(INADDR_ANY);
            }
            else {
                /* Per RFC 2553, accept connections on loopback IF */
                retval =
                        inet_pton(AF_INET, "127.0.0.1", &(sin.sin_addr.s_addr));
                if (retval <= 0) {
                    return (EAI_SYSTEM);
                }
            }

            /*
             * Create addrinfo struct(s) containing this IPv4 address. If ai
             * is NULL, this will be caught at end of getaddrinfo
             */
            ai = setAddrInfo((struct sockaddr *)&sin, AF_INET, service,
                    hints);
        }

#ifdef _INCLUDE_IPv6_CODE
        if (hints->ai_family == AF_INET6 || hints->ai_family == AF_UNSPEC) {
            if (hints->ai_flags & AI_PASSIVE) {
                /* Per RFC 2553, accept connections on any IF */
                memcpy(&(sin6.sin6_addr),
                        (void *)(&IPV6_UNSPECIFIED_ADDRESS), sizeof(IP6N));
            }
            else {
                /* Per RFC 2553, accept connections on loopback IF */
                memcpy(&(sin6.sin6_addr),
                        (void *)(&IPV6_LOOPBACK_ADDRESS), sizeof(IP6N));
            }

            /*
             * Create addrinfo struct(s) containing this IPv6 address. If ai
             * is NULL, this will be caught at end of getaddrinfo
             */
            aiTmp = setAddrInfo((struct sockaddr *)&sin6, AF_INET6, service,
                    hints);

            if (aiTmp) {
                /*
                 * The current list (ai) may not be empty.  Merge the new
                 * results (aiTmp) into the existing ai to handle this case.
                 */
                mergeLists(&ai, &aiTmp);
            }
        }
#endif
    }

    /* Give user our allocated and initialized addrinfo struct(s) */
    *res = ai;

    if (!ai) {
        /* Our list is empty - memory allocations failed */
        return (EAI_MEMORY);
    }

    return (0);
}

/*
 * setAddrInfo()
 * Intermediate step to handle permutations of ai_socktype and ai_protocol
 * hints fields, passed by the user.  If both socktype and protocol are 0, must
 * create a results struct for each socktype and protocol.
 * Returns an empty list (NULL) or a list with one or more nodes.
 */
static struct addrinfo *setAddrInfo(struct sockaddr *addr, int family,
        const char *service, const struct addrinfo *hints)
{
    struct addrinfo *ai = NULL;
    struct addrinfo *aiTmp = NULL;

    if ((hints->ai_socktype == 0 && hints->ai_protocol == 0) ||
            (hints->ai_socktype == 0 && hints->ai_protocol == IPPROTO_UDP) ||
            (hints->ai_socktype == SOCK_DGRAM && hints->ai_protocol == 0) ||
            (hints->ai_socktype == SOCK_DGRAM &&
            hints->ai_protocol == IPPROTO_UDP)) {

        ai = createAddrInfo(addr, family, service, hints->ai_flags, SOCK_DGRAM,
                IPPROTO_UDP);
    }

    if ((hints->ai_socktype == 0 && hints->ai_protocol == 0) ||
            (hints->ai_socktype == 0 && hints->ai_protocol == IPPROTO_TCP) ||
            (hints->ai_socktype == SOCK_STREAM && hints->ai_protocol == 0) ||
            (hints->ai_socktype == SOCK_STREAM &&
            hints->ai_protocol == IPPROTO_TCP)) {

        aiTmp = createAddrInfo(addr, family, service, hints->ai_flags, SOCK_STREAM,
                IPPROTO_TCP);

        if (aiTmp) {
            /* Insert into front of list (assume UDP node was added above) */
            aiTmp->ai_next = ai;
            ai = aiTmp;
        }
    }

    return (ai);
}

/*
 * createAddrInfo()
 * Create new address info structure. Returns a single node.
 */
static struct addrinfo *createAddrInfo(struct sockaddr *addr, int family,
        const char *service, int flags, int socktype, int protocol)
{
    struct addrinfo *ai = NULL;

    /*
     * Allocate memory for the addrinfo struct, which we must fill out and
     * return to the caller.  This struct also has a pointer to a generic socket
     * address struct, which will point to either struct sockaddr_in, or
     * struct sockaddr_in6, depending.  Need to allocate enough space to hold
     * that struct, too.
     */
    if (!(ai = (struct addrinfo *)malloc(NDK_ADDRINFO_ALLOCSZ))) {
        DbgPrintf(DBG_WARN,
                "createAddrInfo: Error: memory allocation failed\n");
        return (NULL);
    }

    /* zero out our allocation */
    memset(ai, 0, NDK_ADDRINFO_ALLOCSZ);

    ai->ai_flags = flags;
    ai->ai_socktype = socktype;
    ai->ai_protocol = protocol;
    ai->ai_canonname = NULL;
    ai->ai_next = NULL;

    /* Store socket addr struct after the addrinfo struct in our memory block */
    ai->ai_addr = (struct sockaddr *)(ai + 1);

    if (family == AF_INET) {
        /* Fill in structure for IPv4 */
        ai->ai_family = AF_INET;
        ai->ai_addrlen = sizeof(struct sockaddr_in);

        /* Write values to addrinfo's socket struct as an sockaddr_in struct */
        ((struct sockaddr_in *)ai->ai_addr)->sin_family = AF_INET;

        ((struct sockaddr_in *)ai->ai_addr)->sin_port = NDK_htons(atoi(service));

        ((struct sockaddr_in *)ai->ai_addr)->sin_addr =
                ((struct sockaddr_in *)addr)->sin_addr;
    }
#ifdef _INCLUDE_IPv6_CODE
    else {
        /* Fill in structure for IPv6 */
        ai->ai_family = AF_INET6;
        ai->ai_addrlen = sizeof(struct sockaddr_in6);

        /* Write values to addrinfo's socket struct as an sockaddr_in6 struct */
        ((struct sockaddr_in6 *)ai->ai_addr)->sin6_family = AF_INET6;

        ((struct sockaddr_in6 *)ai->ai_addr)->sin6_port = NDK_htons(atoi(service));

        memcpy(&(((struct sockaddr_in6 *)ai->ai_addr)->sin6_addr),
                &(((struct sockaddr_in6 *)addr)->sin6_addr),
                sizeof(struct in6_addr));

        /* use interface 1 by default. User can change in their app if needed */
        ((struct sockaddr_in6 *)ai->ai_addr)->sin6_scope_id = 1;
    }
#endif

    return (ai);
}

/*
 * mergeLists()
 * Combines an existing linked list of addrinfo structs (curList) and a newly
 * obtained list (newList) into a single list. If the existing list is empty,
 * it will be initialized to the new list.  If both lists are empty, no action
 * is taken.
 */
static int mergeLists(struct addrinfo **curList, struct addrinfo **newList)
{
    int numNodes = 0;
    struct addrinfo *tail = NULL;

    /* Check params */
    if (!curList || !newList) {
        return (numNodes);
    }

    /* Update node count & find end of new list */
    for (tail = *newList; tail != NULL;) {
        numNodes++;
        if (tail->ai_next != NULL) {
            /* Not the tail, keep traversing */
            tail = tail->ai_next;
        }
        else {
            /* Tail found, quit loop */
            break;
        }
    }

    /* Append current list to end of new list */
    tail->ai_next = *curList;
    *curList = *newList;

    return (numNodes);
}

/*
 * freeaddrinfo()
 * Free the addrinfo struct allocated by calling getaddrinfo
 */
void freeaddrinfo(struct addrinfo *res)
{
    struct addrinfo *aiTmp;

    /* Delete all nodes in linked list */
    while (res) {
        aiTmp = res->ai_next;
        free((void *)res);
        res = aiTmp;
    }
}
