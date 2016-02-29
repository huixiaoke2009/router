#ifndef _COMMON_H
#define _COMMON_H

#include <arpa/inet.h>
#include <stdarg.h>
#include <assert.h>
#include <string>
using namespace std;

#define NYIR do{return false;}while(0)

#define Timestamp struct tm
#define STR(x) (((string)(x)).c_str())
//lxf delete
#define ASSERT(...) do{assert(false);abort();}while(0)
//#define ASSERT(...) do{NULL;}while(0)


#define AMF3_TRAITS "_t_"
#define AMF3_TRAITS_LEN 14
#define AMF3_TRAITS_CLASSNAME "____class_name____"
#define AMF3_TRAITS_CLASSNAME_LEN 18
#define AMF3_TRAITS_DYNAMIC "____isDynamic____"
#define AMF3_TRAITS_DYNAMIC_LEN 17

#define Timestamp_init {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}

#define STR(x) (((string)(x)).c_str())
#define MAP_HAS1(m,k) ((bool)((m).find((k))!=(m).end()))
#define MAP_HAS2(m,k1,k2) ((MAP_HAS1((m),(k1))==true)?MAP_HAS1((m)[(k1)],(k2)):false)
#define MAP_HAS3(m,k1,k2,k3) ((MAP_HAS1((m),(k1)))?MAP_HAS2((m)[(k1)],(k2),(k3)):false)
#define FOR_MAP(m,k,v,i) for(map< k , v >::iterator i=(m).begin();i!=(m).end();i++)
#define MAP_KEY(i) ((i)->first)
#define MAP_VAL(i) ((i)->second)

#define ADD_VECTOR_END(v,i) (v).push_back((i))
#define FOR_VECTOR_ITERATOR(e,v,i) for(vector<e>::iterator i=(v).begin();i!=(v).end();i++)
#define VECTOR_VAL(i) (*(i))


#define PRId64 "lu"
#define PRIu64 "lu"
#define PRIu32 "u"
#define PRIu16 "u"
#define PRIu8 "u"
#define PRIz ""

#define INT64_MAX  ((int64_t)(0x7fffffffffffffffLL))
#define UINT32_MAX ((uint32_t)(0xffffffffUL))
#define INT32_MAX  ((int32_t)(0x7fffffffL))
#define INT32_MIN  ((int32_t)(0x80000000L))
#define UINT16_MAX ((uint16_t)(0xffff))
#define INT16_MAX  ((int16_t)(0x7fff))
#define INT16_MIN  ((int16_t)(0x8000))
#define UINT8_MAX  ((uint8_t)(0xff))
#define INT8_MAX   ((int8_t)(0x7f))
#define INT8_MIN   ((int8_t)(0x80))

#define VAR_INDEX_VALUE_LEN 16

//64 bit
#ifndef DONT_DEFINE_HTONLL
#define htonll(x)    (x)
#define ntohll(x)    (x)
#endif /* DONT_DEFINE_HTONLL */

//64 bit
#define EHTONLL(x) htonll(x)
#define ENTOHLL(x) ntohll(x)

//double
#define EHTOND(hostDoubleVal,networkUI64Val) networkUI64Val=EHTONLL((*((uint64_t *)(&(hostDoubleVal)))))
#define ENTOHD(networkUI64Val,hostDoubleVal) \
do {\
    uint64_t ___tempHostENTOHD=ENTOHLL(networkUI64Val); \
    hostDoubleVal=(double)(*((double *)&___tempHostENTOHD)); \
} while(0)

//32 bit
#define EHTONL(x) htonl(x)
#define ENTOHL(x) ntohl(x)

//16 bit
#define EHTONS(x) htons(x)
#define ENTOHS(x) ntohs(x)

#define ENTOHSP(pNetworkPointer) ENTOHS(*((uint16_t *)(pNetworkPointer)))
#define ENTOHLP(pNetworkPointer) ENTOHL(*((uint32_t *)(pNetworkPointer)))

//64 bit pointer
#define EHTONLLP(pNetworkPointer,hostLongLongValue) (*((uint64_t*)(pNetworkPointer)) = EHTONLL(hostLongLongValue))
#define ENTOHLLP(pNetworkPointer) ENTOHLL(*((uint64_t *)(pNetworkPointer)))

//double pointer
#define EHTONDP(hostDoubleVal,pNetworkPointer) EHTOND(hostDoubleVal,(*((uint64_t *)(pNetworkPointer))))
#define ENTOHDP(pNetworkPointer,hostDoubleVal) ENTOHD((*((uint64_t *)(pNetworkPointer))),hostDoubleVal)

string vFormat(string fmt, va_list args);
string format(string fmt, ...);

string lowerCase(string value);
string upperCase(string value);

bool isNumeric(string value);
void replace(string &target, string search, string replacement);


#endif
