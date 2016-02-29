
#ifndef _COMMON_H_
#define _COMMON_H_

const int XY_MAXBUFF_LEN = 2048000;   //最大允许接收数据的长度为2M
const int XY_PKG_MAX_LEN = 2048000;   //单个协议包最大的长度为2M
const int XY_MAX_CONN_NUM = 1000;     //最大连接数
const int XY_MAX_GROUP_NUM = 32;      //每个group最多拥有的svr数量
const int XY_GROUP_MOD = 100;         //如果目标id能够模这个值为0，表示其是一个group
const int XY_MAX_ROUTER_NUM = 10;     //最大Router数

#endif