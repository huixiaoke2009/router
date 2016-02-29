#ifndef _TIMER_POOL_H_
#define _TIMER_POOL_H_

#include <limits.h>
#include <sys/time.h>
#include <vector>

#include "mem_pool/fixedsize_mem_pool.h"
#include "util/util.h"
#include "log/log.h"

#define TIMER_ELEMENT 10000   //定时器最小粒度10ms
namespace mmlib
{
template<class DATATYPE>
class CTimerPool
{
public:
    const static int SUCCESS = 0;
    const static int ERROR = -1;

    const static int POOL_FULL = 101;
    const static int TIMER_NOT_EXIST = 102;

    const static int TB1_BIT = 8;
    const static int TB2_BIT = 6;
    const static int TB3_BIT = 6;
    const static int TB4_BIT = 6;
    const static int TB5_BIT = 6;

    const static int TB1_NUM = 1 << TB1_BIT;
    const static int TB2_NUM = 1 << TB2_BIT;
    const static int TB3_NUM = 1 << TB3_BIT;
    const static int TB4_NUM = 1 << TB4_BIT;
    const static int TB5_NUM = 1 << TB5_BIT;
    const static int TBALL_NUM = TB1_NUM + TB2_NUM + TB3_NUM + TB4_NUM + TB5_NUM;

public:
    typedef struct tagTimerPoolHeader
    {
        unsigned int uiMagicNumber;
        struct timeval tLastScanTime;  //上次扫描时间，精确到微秒
        unsigned int uiTB1CurPt; //第一个TB组的当前TB偏移
        unsigned int uiTB2CurPt; //第二个TB组的当前TB偏移
        unsigned int uiTB3CurPt; //第三个TB组的当前TB偏移
        unsigned int uiTB4CurPt; //第四个TB组的当前TB偏移
        unsigned int uiTB5CurPt; //第五个TB组的当前TB偏移
        int iTimerNodeNum; //定时器的个数
        char szUnUsed[92];
    } TimerPoolHeader;

    //双向链表头，或者叫定时器的桶，指针记录的都是偏移量
    //由于TimerBucket的前两个字段排序与TimerNode前两个字段排序一致，可以通过TimerNode的指针访问这两个字段
    typedef struct tagTimerBucket
    {
        unsigned int uiNext;
        unsigned int uiPrev;
    } TimerBucket;

    //双向链表节点，用于存放实际Timer的数据
    typedef struct tagTimerNode
    {
        unsigned int uiNext;
        unsigned int uiPrev;
        struct timeval tExpire; //到期时间，精确到微秒
        unsigned int uiRandom; //防止误删Timer的随机数
        DATATYPE TimerParam;
    } TimerNode;

private:
    TimerPoolHeader *m_pstTPH; //TimerPoolHeader;

    TimerBucket *m_pstTB1; //256个桶
    TimerBucket *m_pstTB2; //64个桶
    TimerBucket *m_pstTB3; //64个桶
    TimerBucket *m_pstTB4; //64个桶
    TimerBucket *m_pstTB5; //64个桶

    jmlib::CFixedsizeMemPool<TimerNode> m_objTimerNodePool;

    char *m_pMem; //指向存储空间的指针
    int m_iMemSize; //存储空间的大小

    char m_szErrMsg[256];
    bool m_bInitFlag;

public:
    CTimerPool()
    {
        m_pstTPH = NULL;

        m_pstTB1 = NULL;
        m_pstTB2 = NULL;
        m_pstTB3 = NULL;
        m_pstTB4 = NULL;
        m_pstTB5 = NULL;

        memset(m_szErrMsg, 0x0, sizeof(m_szErrMsg));

        m_bInitFlag = false;
    }

    ~CTimerPool()
    {

    }

    const char *GetErrMsg()
    {
        return m_szErrMsg;
    }

    /**
     * @brief 初始化整个TimerPool
     * @param pvMem 内存头结点
     * @param iMemSize 开辟空间的大小
     * @param iClearFlag 是否清空标志位
     *
     * @return 0-成功 其他-失败
     */
    int Init(void *pvMem, int iMemSize, int iClearFlag = 0)
    {
        int iRetVal = 0;
        if (iMemSize <= (int) ((sizeof(TimerBucket) * TBALL_NUM) + sizeof(TimerPoolHeader)))
        {
            snprintf(m_szErrMsg, sizeof(m_szErrMsg), "memsize is not enough, head+tbsize=%d, memsize=%d", (int) ((sizeof(TimerBucket) * TBALL_NUM) + sizeof(TimerPoolHeader)), iMemSize);
            return ERROR;
        }

        m_pMem = (char*) pvMem;
        m_pstTPH = (TimerPoolHeader *) m_pMem;

        m_pstTB1 = (TimerBucket *) (m_pMem + sizeof(TimerPoolHeader));
        m_pstTB2 = m_pstTB1 + TB1_NUM;
        m_pstTB3 = m_pstTB2 + TB2_NUM;
        m_pstTB4 = m_pstTB3 + TB3_NUM;
        m_pstTB5 = m_pstTB4 + TB4_NUM;

        void *pFSMPMem = m_pMem + sizeof(TimerPoolHeader) + (sizeof(TimerBucket) * TBALL_NUM);
        int iFSMPMemSize = iMemSize - sizeof(TimerPoolHeader) - (sizeof(TimerBucket) * TBALL_NUM);

        iRetVal = m_objTimerNodePool.Init(pFSMPMem, iFSMPMemSize, 0, iClearFlag);
        if (iRetVal != 0)
        {
            snprintf(m_szErrMsg, sizeof(m_szErrMsg), "init timer_node_pool failed, timer_node_memsize=%d, ret=%d", iFSMPMemSize, iRetVal);
            return ERROR;
        }

        if (iClearFlag == 1)
        {
            //初始化TimerPool
            memset(m_pstTPH, 0x0, sizeof(TimerPoolHeader));
            memset(m_pstTB1, 0x0, sizeof(TimerBucket) * TBALL_NUM);

            m_pstTPH->uiMagicNumber = 0x1234ABCD;
            gettimeofday(&m_pstTPH->tLastScanTime, NULL);
            m_pstTPH->tLastScanTime.tv_usec /= TIMER_ELEMENT;
            m_pstTPH->tLastScanTime.tv_usec *= TIMER_ELEMENT;
        }

        Log(0, 0, LOG_INFO, "init timer_node_pool succ, pool_node_num=%d, used_pool_node_num=%d, timer_node_num=%d", m_objTimerNodePool.GetNodeNum(), m_objTimerNodePool.GetUsedNodeNum(), m_pstTPH->iTimerNodeNum);

        m_iMemSize = iMemSize;

        m_bInitFlag = true;

        return SUCCESS;
    }

    /**
     * @brief 添加一个定时器到TimerPool中
     * @param uiInterval 定时器触发时间间隔，单位ms
     * @param TimerParam 定时器附带的参数，定时器触发时会返回该参数
     * @param ullTimerID 定时器ID，该ID用于唯一确定一个定时器
     *
     * @return 0-成功 其他-失败
     */
    int AddTimer(unsigned int uiInterval, const DATATYPE &TimerParam, unsigned long long *pullTimerID)
    {
        if (!m_bInitFlag)
        {
            snprintf(m_szErrMsg, sizeof(m_szErrMsg), "timer_pool not inited.");
            return ERROR;
        }

        if (uiInterval == 0)
        {
            snprintf(m_szErrMsg, sizeof(m_szErrMsg), "timer interval is not valid, interval=%d", uiInterval);
            return ERROR;
        }

        TimerNode *pstTimerNode = m_objTimerNodePool.AllocateNode();
        if (pstTimerNode == NULL)
        {
            snprintf(m_szErrMsg, sizeof(m_szErrMsg), "allocate timer_node failed, pool full.");
            return POOL_FULL;
        }

        TimerBucket *pstTimerBucket = NULL;
        unsigned int uiIntervalForTB = uiInterval / (TIMER_ELEMENT / 1000);

        if ((uiIntervalForTB >> TB1_BIT) == 0)
        {
            //FOR TB1
            pstTimerBucket = m_pstTB1 + ((m_pstTPH->uiTB1CurPt + uiIntervalForTB) % TB1_NUM);
            Log(0, 0, LOG_DEBUG, "%s|interval|%d|TB1|%d,%d", __func__, uiInterval, m_pstTPH->uiTB1CurPt, ((m_pstTPH->uiTB1CurPt + uiIntervalForTB) % TB1_NUM));
        }
        else
        {
            uiIntervalForTB -= (TB1_NUM - m_pstTPH->uiTB1CurPt);
            if ((uiIntervalForTB >> (TB1_BIT + TB2_BIT)) == 0)
            {
                //FOR TB2
                pstTimerBucket = m_pstTB2 + ((m_pstTPH->uiTB2CurPt + (uiIntervalForTB >> TB1_BIT)) % TB2_NUM);
                Log(0, 0, LOG_DEBUG, "%s|interval|%d|TB2|%d,%d", __func__, uiInterval, m_pstTPH->uiTB2CurPt, ((m_pstTPH->uiTB2CurPt + (uiIntervalForTB >> TB1_BIT)) % TB2_NUM));
            }
            else
            {
                uiIntervalForTB -= ((TB2_NUM - m_pstTPH->uiTB2CurPt) << TB1_BIT);
                if ((uiIntervalForTB >> (TB1_BIT + TB2_BIT + TB3_BIT)) == 0)
                {
                    //FOR TB3
                    pstTimerBucket = m_pstTB3 + ((m_pstTPH->uiTB3CurPt + (uiIntervalForTB >> (TB1_BIT + TB2_BIT))) % TB3_NUM);
                    Log(0, 0, LOG_DEBUG, "%s|interval|%d|TB3|%d,%d", __func__, uiInterval, m_pstTPH->uiTB3CurPt, ((m_pstTPH->uiTB3CurPt + (uiIntervalForTB >> (TB1_BIT + TB2_BIT))) % TB3_NUM));
                }
                else
                {
                    uiIntervalForTB -= ((TB3_NUM - m_pstTPH->uiTB3CurPt) << (TB1_BIT + TB2_BIT));
                    if ((uiIntervalForTB >> (TB1_BIT + TB2_BIT + TB3_BIT + TB4_BIT)) == 0)
                    {
                        //FOR TB4
                        pstTimerBucket = m_pstTB4 + ((m_pstTPH->uiTB4CurPt + (uiIntervalForTB >> (TB1_BIT + TB2_BIT + TB3_BIT))) % TB4_NUM);
                        Log(0, 0, LOG_DEBUG, "%s|interval|%d|TB4|%d,%d", __func__, uiInterval, m_pstTPH->uiTB4CurPt, ((m_pstTPH->uiTB4CurPt + (uiIntervalForTB >> (TB1_BIT + TB2_BIT + TB3_BIT))) % TB4_NUM));
                    }
                    else
                    {
                        //FOR TB5
                        uiIntervalForTB -= ((TB4_NUM - m_pstTPH->uiTB4CurPt) << (TB1_BIT + TB2_BIT + TB3_BIT));
                        pstTimerBucket = m_pstTB5 + ((m_pstTPH->uiTB5CurPt + (uiIntervalForTB >> (TB1_BIT + TB2_BIT + TB3_BIT + TB4_BIT))) % TB5_NUM);
                        Log(0, 0, LOG_DEBUG, "%s|interval|%d|TB5|%d,%d", __func__, uiInterval, m_pstTPH->uiTB5CurPt, ((m_pstTPH->uiTB5CurPt + (uiIntervalForTB >> (TB1_BIT + TB2_BIT + TB3_BIT + TB4_BIT))) % TB5_NUM));
                    }

                }

            }
        }

        struct timeval stTimeExpire, stTimeNow;
        gettimeofday(&stTimeNow, NULL); //按照内核定时器的实现机制，这里是不需要记录到达时间的，这里暂时保留着
        stTimeExpire = stTimeNow;

        stTimeExpire.tv_sec += (uiInterval / 1000);
        if ((stTimeExpire.tv_usec + ((uiInterval % 1000) * 1000)) < 1000000)
        {
            stTimeExpire.tv_usec += ((uiInterval % 1000) * 1000);
        }
        else
        {
            stTimeExpire.tv_sec += 1;
            stTimeExpire.tv_usec += (((uiInterval % 1000) * 1000) - 1000000);
        }

        Log(0, 0, LOG_DEBUG, "timer_pool|%s|TB_OFF=%d|NOW=%ld.%06ld|EXPIRE=%ld.%06ld", __func__, (int) (pstTimerBucket - m_pstTB1), stTimeNow.tv_sec, stTimeNow.tv_usec, stTimeExpire.tv_sec, stTimeExpire.tv_usec);

        memcpy(&(pstTimerNode->tExpire), &stTimeExpire, sizeof(stTimeExpire));
        pstTimerNode->uiRandom = jmlib::CRandomTool::Instance()->Get(0, INT_MAX);
        memcpy(&(pstTimerNode->TimerParam), &TimerParam, sizeof(DATATYPE));
        pstTimerNode->uiPrev = ((char *) pstTimerBucket - m_pMem);
        pstTimerNode->uiNext = pstTimerBucket->uiNext;

        TimerNode *pstLastTimeNode = NULL;
        if (pstTimerBucket->uiNext > 0)
        {
            pstLastTimeNode = (TimerNode *) (m_pMem + pstTimerBucket->uiNext);
        }
        pstTimerBucket->uiNext = ((char *) pstTimerNode - m_pMem);
        if (pstLastTimeNode)
        {
            pstLastTimeNode->uiPrev = pstTimerBucket->uiNext;
        }

        memcpy(pullTimerID, &pstTimerBucket->uiNext, sizeof(int));
        memcpy(((char *) pullTimerID) + 4, &pstTimerNode->uiRandom, sizeof(int));

        m_pstTPH->iTimerNodeNum++;

        Log(0, 0, LOG_DEBUG, "timer_pool|%s|timer_node_num=%d", __func__, m_pstTPH->iTimerNodeNum);

        return SUCCESS;
    }

    /**
     * @brief 删除一个定时器
     * @param ullTimerID 定时器ID
     *
     * @return 0-成功 其他-失败
     */
    int DelTimer(unsigned long long ullTimerID)
    {
        if (!m_bInitFlag)
        {
            snprintf(m_szErrMsg, sizeof(m_szErrMsg), "timer_pool not inited.");
            return ERROR;
        }

        if (m_pstTPH->iTimerNodeNum == 0)
        {
            snprintf(m_szErrMsg, sizeof(m_szErrMsg), "timer pool is empty, timer_id=%llu", ullTimerID);
            return TIMER_NOT_EXIST;
        }

        int iRetVal = 0;

        int iRandom = 0;
        int iOffSet = 0;

        memcpy(&iOffSet, &ullTimerID, sizeof(int));
        memcpy(&iRandom, ((char *) &ullTimerID) + 4, sizeof(int));

        if ((iOffSet <= 0) || (iOffSet >= m_iMemSize))
        {
            snprintf(m_szErrMsg, sizeof(m_szErrMsg), "timer_id is not valid, node offset=%d, memsize=%d.", iOffSet, m_iMemSize);
            return ERROR;
        }

        TimerNode *pstTimerNode = (TimerNode *) (m_pMem + iOffSet);

        if (pstTimerNode->uiPrev == 0)
        {
            snprintf(m_szErrMsg, sizeof(m_szErrMsg), "timer_id is not valid, node prev is 0, timer_id=%llu.", ullTimerID);
            return ERROR;
        }

        if (pstTimerNode->uiRandom != (unsigned int) iRandom)
        {
            snprintf(m_szErrMsg, sizeof(m_szErrMsg), "timer_id is not valid, random is not equ, node_random=%d, id_random=%d", pstTimerNode->uiRandom, iRandom);
            return TIMER_NOT_EXIST;
        }

        TimerNode *pstPrevTimerNode = (TimerNode *) (m_pMem + pstTimerNode->uiPrev);
        TimerNode *pstNextTimerNode = NULL;
        if (pstTimerNode->uiNext > 0)
        {
            pstNextTimerNode = (TimerNode *) (m_pMem + pstTimerNode->uiNext);
        }

        pstPrevTimerNode->uiNext = pstTimerNode->uiNext;

        if (pstNextTimerNode)
        {
            pstNextTimerNode->uiPrev = pstTimerNode->uiPrev;
        }

        m_pstTPH->iTimerNodeNum--;
        if (m_pstTPH->iTimerNodeNum < 0)
        {
            //TODO 这个是非常致命的错误
            m_pstTPH->iTimerNodeNum = 0;
        }

        iRetVal = m_objTimerNodePool.ReleaseNode(pstTimerNode);
        if (iRetVal != 0)
        {
            snprintf(m_szErrMsg, sizeof(m_szErrMsg), "release timer_node failed, ret=%d", iRetVal);
            return ERROR;
        }

        return SUCCESS;
    }

    /**
     * @brief 将pstSrcTimerBucket上的定时器节点，迁移到pstDstTimerBucket后面iDstTimerBucketNum个对应的定时器桶中
     * @param pstDstTimerBucket目标定时器桶的首指针
     * @param iDstTimerBucketNum目标定时器桶的个数
     * @param iDstMaskBits目标计算桶偏移的时候，末尾掩码的bit数量
     * @param pstSrcTimerBucket需要迁移的定时器桶
     * @return 0-成功 其他-失败
     */
    int Migrate(TimerBucket *pstDstTimerBucket, int iDstTimerBucketNum, int iDstMaskBits, TimerBucket *pstSrcTimerBucket)
    {
        if (pstSrcTimerBucket->uiNext == 0)
        {
            //这个链上面没有节点，不需要迁移
            return SUCCESS;
        }

        TimerNode *pstTimerNode = (TimerNode *) (m_pMem + pstSrcTimerBucket->uiNext);
        TimerNode *pstPrevTimerNode = (TimerNode *) (m_pMem + pstTimerNode->uiPrev);
        TimerNode *pstNextTimerNode = (pstTimerNode->uiNext > 0) ? (TimerNode *) (m_pMem + pstTimerNode->uiNext) : NULL;

        while (pstTimerNode != NULL)
        {
            //转移节点
            struct timeval stTimeNow;
            gettimeofday(&stTimeNow, NULL);
            unsigned int uiInterval = 0;

            //找到正确的TB
            if (timercmp(&stTimeNow, &pstTimerNode->tExpire, <))
            {
                //计算出到期时间
                uiInterval = (pstTimerNode->tExpire.tv_sec - stTimeNow.tv_sec) * (1000000 / TIMER_ELEMENT) + ((pstTimerNode->tExpire.tv_usec - stTimeNow.tv_usec) / TIMER_ELEMENT);
            }
            else
            {
                Log(0, 0, LOG_WARN, "timer_pool|%s|EXPIRE=%d.%d|NOW=%d.%d|INTERVAL=%d", __func__, (int) pstTimerNode->tExpire.tv_sec, (int) pstTimerNode->tExpire.tv_usec, (int) stTimeNow.tv_sec, (int) stTimeNow.tv_usec, uiInterval);
            }

            Log(0, 0, LOG_DEBUG, "timer_pool|%s|EXPIRE=%d.%d|NOW=%d.%d|INTERVAL=%d", __func__, (int) pstTimerNode->tExpire.tv_sec, (int) pstTimerNode->tExpire.tv_usec, (int) stTimeNow.tv_sec, (int) stTimeNow.tv_usec, uiInterval);

            uiInterval = uiInterval >> iDstMaskBits;

            if (uiInterval > 0)
            {
                uiInterval -= 1;
            }

            if ((int) uiInterval >= iDstTimerBucketNum)
            {
                Log(0, 0, LOG_ERR, "TimePool|%s|%d|node interval is too big, interval=%d", __func__, __LINE__, uiInterval);
                uiInterval = iDstTimerBucketNum - 1;
            }

            //删除节点
            pstPrevTimerNode->uiNext = pstTimerNode->uiNext;
            if (pstNextTimerNode)
            {
                pstNextTimerNode->uiPrev = pstTimerNode->uiPrev;
            }

            //插入节点
            TimerBucket *pstTmpTB = pstDstTimerBucket + uiInterval;
            pstTimerNode->uiNext = pstTmpTB->uiNext;
            pstTimerNode->uiPrev = ((char *) pstTmpTB - m_pMem);
            if (pstTmpTB->uiNext > 0)
            {
                TimerNode *pstTmpTN = (TimerNode *) (m_pMem + pstTmpTB->uiNext);
                pstTmpTN->uiPrev = ((char *) pstTimerNode - m_pMem);
            }
            pstTmpTB->uiNext = ((char *) pstTimerNode - m_pMem);

            pstTimerNode = pstNextTimerNode;
            pstNextTimerNode = NULL;
            if (pstTimerNode != NULL)
            {
                pstNextTimerNode = (pstTimerNode->uiNext > 0) ? (TimerNode *) (m_pMem + pstTimerNode->uiNext) : NULL;
            }
        }

        return SUCCESS;
    }

    /**
     * @brief 获得当前已经触发的所有定时器信息
     * @param vAllTimerParam 所有当前需要触发的定时器信息vector
     * @return 0-成功 其他-失败
     *
     * @note 由于该接口驱动了整个时间轮的运转，必须保证每1ms调用一次
     */
    int GetTimerInternal(std::vector<unsigned long long> &vullTimerID, std::vector<DATATYPE> &vAllTimerParam, const struct timeval &tTimeNow)
    {
        int iRetVal = 0;

        //Log(0, 0, LOG_DEBUG, "timer_pool|%s|NODE_NUM|%d|TBPT|%03d %03d %03d %03d %03d", __func__, m_pstTPH->iTimerNodeNum, m_pstTPH->uiTB1CurPt, m_pstTPH->uiTB2CurPt, m_pstTPH->uiTB3CurPt, m_pstTPH->uiTB4CurPt, m_pstTPH->uiTB5CurPt);

        if (m_pstTPH->iTimerNodeNum == 0)
        {
            //没有任何节点，整个定时器转轮停止转动
            return SUCCESS;
        }

        if (m_pstTPH->uiTB1CurPt >= (unsigned int) TB1_NUM)
        {
            Log(0, 0, LOG_ERR, "TimePool|%s|%d|tb1_cur_pt is not valid, tb1_cur_pt=%d", __func__, __LINE__, m_pstTPH->uiTB1CurPt);
            //TODO 这里可以考虑不退出，如果出现这种情况，保证系统还能继续，可以考虑先不退出，只是纠正
            m_pstTPH->uiTB1CurPt = 0;
        }

        TimerBucket *pstTB1 = m_pstTB1 + m_pstTPH->uiTB1CurPt;

        if (pstTB1->uiNext == 0)
        {
            //该链上面没有定时器节点
        }
        else
        {
            TimerNode *pstTimerNode = (TimerNode *) (m_pMem + pstTB1->uiNext);
            //TODO 这里需要对PREV进行检查
            TimerNode *pstPrevTimerNode = (TimerNode *) (m_pMem + pstTimerNode->uiPrev);
            TimerNode *pstNextTimerNode = (pstTimerNode->uiNext > 0) ? (TimerNode *) (m_pMem + pstTimerNode->uiNext) : NULL;

            while (pstTimerNode != NULL)
            {
                vAllTimerParam.push_back(pstTimerNode->TimerParam);
                unsigned long long ullTimerID = 0;
                int iTimerNodeOffset = (char *) pstTimerNode - m_pMem;
                memcpy(&ullTimerID, &iTimerNodeOffset, sizeof(int));
                memcpy(((char *) &ullTimerID) + 4, &pstTimerNode->uiRandom, sizeof(int));
                vullTimerID.push_back(ullTimerID);

                //这里不检查expire时间，每次处理1个链表

                //删除节点
                pstPrevTimerNode->uiNext = pstTimerNode->uiNext;
                if (pstNextTimerNode)
                {
                    pstNextTimerNode->uiPrev = pstTimerNode->uiPrev;
                }

                m_pstTPH->iTimerNodeNum--;
                if (m_pstTPH->iTimerNodeNum < 0)
                {
                    Log(0, 0, LOG_ERR, "TimePool|%s|%d|timer node in header is below zero, chang to zero", __func__, __LINE__);
                    m_pstTPH->iTimerNodeNum = 0;
                }

                iRetVal = m_objTimerNodePool.ReleaseNode(pstTimerNode);
                if (iRetVal != 0)
                {
                    Log(0, 0, LOG_ERR, "TimePool|%s|%d|release node failed, ret=%d", __func__, __LINE__, iRetVal);
                }

                pstTimerNode = pstNextTimerNode;
                pstNextTimerNode = NULL;
                if (pstTimerNode != NULL)
                {
                    pstNextTimerNode = (pstTimerNode->uiNext > 0) ? (TimerNode *) (m_pMem + pstTimerNode->uiNext) : NULL;
                }
            }

            if (m_pstTPH->iTimerNodeNum == 0)
            {
                //如果节点全部删除，时间轮不再转动
                return SUCCESS;
            }
        }

        //进行时间轮转动
        m_pstTPH->uiTB1CurPt++;
        if (m_pstTPH->uiTB1CurPt == (unsigned int) TB1_NUM)
        {
            //节点迁移
            //每次迁移保证TB2上的某条链全部迁移完毕，超过TB1最大时长的，按照TB1中最大市场计算
            if (m_pstTPH->uiTB2CurPt >= (unsigned int) TB2_NUM)
            {
                Log(0, 0, LOG_ERR, "TimePool|%s|%d|tb2_cur_pt is not valid, tb2_cur_pt=%d", __func__, __LINE__, m_pstTPH->uiTB2CurPt);
                //TODO 这里可以考虑不退出，如果出现这种情况，保证系统还能继续，可以考虑先不退出，只是纠正
                m_pstTPH->uiTB2CurPt = 0;
            }

            //TB2->TB1
            Migrate(m_pstTB1, TB1_NUM, 0, m_pstTB2 + m_pstTPH->uiTB2CurPt);
            m_pstTPH->uiTB1CurPt = 0;
            m_pstTPH->uiTB2CurPt++;

            if (m_pstTPH->uiTB2CurPt == (unsigned int) TB2_NUM)
            {
                if (m_pstTPH->uiTB3CurPt >= (unsigned int) TB3_NUM)
                {
                    Log(0, 0, LOG_ERR, "TimePool|%s|%d|tb3_cur_pt is not valid, tb3_cur_pt=%d", __func__, __LINE__, m_pstTPH->uiTB3CurPt);
                    //TODO 这里可以考虑不退出，如果出现这种情况，保证系统还能继续，可以考虑先不退出，只是纠正
                    m_pstTPH->uiTB3CurPt = 0;
                }

                //TB3->TB2
                Migrate(m_pstTB2, TB2_NUM, TB1_BIT, m_pstTB3 + m_pstTPH->uiTB3CurPt);
                m_pstTPH->uiTB2CurPt = 0;
                m_pstTPH->uiTB3CurPt++;

                if (m_pstTPH->uiTB3CurPt == (unsigned int) TB3_NUM)
                {
                    if (m_pstTPH->uiTB4CurPt >= (unsigned int) TB4_NUM)
                    {
                        Log(0, 0, LOG_ERR, "TimePool|%s|%d|tb4_cur_pt is not valid, tb4_cur_pt=%d", __func__, __LINE__, m_pstTPH->uiTB4CurPt);
                        //TODO 这里可以考虑不退出，如果出现这种情况，保证系统还能继续，可以考虑先不退出，只是纠正
                        m_pstTPH->uiTB4CurPt = 0;
                    }

                    //TB4->TB3
                    Migrate(m_pstTB3, TB3_NUM, TB2_BIT + TB1_BIT, m_pstTB4 + m_pstTPH->uiTB4CurPt);
                    m_pstTPH->uiTB3CurPt = 0;
                    m_pstTPH->uiTB4CurPt++;

                    if (m_pstTPH->uiTB4CurPt == (unsigned int) TB4_NUM)
                    {
                        if (m_pstTPH->uiTB5CurPt >= (unsigned int) TB5_NUM)
                        {
                            Log(0, 0, LOG_ERR, "TimePool|%s|%d|tb5_cur_pt is not valid, tb5_cur_pt=%d", __func__, __LINE__, m_pstTPH->uiTB5CurPt);
                            //TODO 这里可以考虑不退出，如果出现这种情况，保证系统还能继续，可以考虑先不退出，只是纠正
                            m_pstTPH->uiTB5CurPt = 0;
                        }

                        //TB5->TB4
                        Migrate(m_pstTB4, TB4_NUM, TB3_BIT + TB2_BIT + TB1_BIT, m_pstTB5 + m_pstTPH->uiTB5CurPt);
                        m_pstTPH->uiTB4CurPt = 0;
                        m_pstTPH->uiTB5CurPt++;

                        m_pstTPH->uiTB5CurPt = m_pstTPH->uiTB5CurPt % TB5_NUM;
                    }
                }
            }
        }

        return SUCCESS;
    }

    /**
     * @brief 获得当前已经触发的所有定时器信息
     * @param vAllTimerParam 所有当前需要触发的定时器信息vector
     * @return 0-成功 其他-失败
     *
     * @note 由于该接口驱动了整个时间轮的运转，需要放在程序的主循环中调用
     */
    int GetTimer(std::vector<unsigned long long> &vullTimerID, std::vector<DATATYPE> &vAllTimerParam)
    {
        if (!m_bInitFlag)
        {
            snprintf(m_szErrMsg, sizeof(m_szErrMsg), "timer_pool not inited.");
            return ERROR;
        }

        int iRetVal = 0;
        int iLoopTimes = 0;
        struct timeval tTimeNow;
        struct timeval tLastTime = m_pstTPH->tLastScanTime;

        gettimeofday(&tTimeNow, NULL);
        tTimeNow.tv_usec /= TIMER_ELEMENT;
        tTimeNow.tv_usec *= TIMER_ELEMENT;

        struct timeval tTimeDiff;
        timersub(&tTimeNow, &m_pstTPH->tLastScanTime, &tTimeDiff);

        if (tTimeDiff.tv_sec > (INT_MAX / (1000000/TIMER_ELEMENT)))
        {
            //TODO: 这里需要判断tTimeDiff超大的情况，将所有定时器清零
            Log(0, 0, LOG_WARN, "%s|%d|time_diff_sec|%ld|clear timer poll", __func__, __LINE__, tTimeDiff.tv_sec);
            return Init(m_pMem, m_iMemSize, 1);
        }

        if ((tTimeDiff.tv_sec > 0)||(tTimeDiff.tv_usec >= TIMER_ELEMENT))
        {
            m_pstTPH->tLastScanTime = tTimeNow;
            iLoopTimes = (tTimeDiff.tv_sec * (1000000/TIMER_ELEMENT)) + (tTimeDiff.tv_usec / TIMER_ELEMENT);
        }

        //Log(0, 0, LOG_DEBUG, "%s|%d|last|%ld.%06ld|now|%ld.%06ld|diff|%ld.%06ld|loop|%d", __func__, __LINE__, tLastTime.tv_sec, tLastTime.tv_usec, tTimeNow.tv_sec, tTimeNow.tv_usec, tTimeDiff.tv_sec, tTimeDiff.tv_usec, iLoopTimes);
        for(int i=0; i<iLoopTimes; i++)
        {
            iRetVal = GetTimerInternal(vullTimerID, vAllTimerParam, tTimeNow);
            if (iRetVal != 0)
            {
                return iRetVal;
            }
        }

        return 0;
    }

    /**
     * @brief 获取一个指定定时器的信息
     * @param ullTimerID 定时器ID
     * @param stTimerNode 定时器的信息
     *
     * @return 0-成功 其他-失败
     */
    int GetTimerInfo(unsigned long long ullTimerID, TimerNode& stTimerNode)
    {
        if (!m_bInitFlag)
        {
            snprintf(m_szErrMsg, sizeof(m_szErrMsg), "timer_pool not inited.");
            return ERROR;
        }

        if (m_pstTPH->iTimerNodeNum == 0)
        {
            snprintf(m_szErrMsg, sizeof(m_szErrMsg), "timer pool is empty, timer_id=%llu", ullTimerID);
            return TIMER_NOT_EXIST;
        }


        int iRandom = 0;
        int iOffSet = 0;

        memcpy(&iOffSet, &ullTimerID, sizeof(int));
        memcpy(&iRandom, ((char *) &ullTimerID) + 4, sizeof(int));

        if ((iOffSet <= 0) || (iOffSet >= m_iMemSize))
        {
            snprintf(m_szErrMsg, sizeof(m_szErrMsg), "timer_id is not valid, node offset=%d, memsize=%d.", iOffSet, m_iMemSize);
            return ERROR;
        }

        memcpy(stTimerNode, m_pMem + iOffSet, sizeof(stTimerNode));

        if (iRandom != (int)stTimerNode.uiRandom)
        {
            snprintf(m_szErrMsg, sizeof(m_szErrMsg), "random id is not equ, random_in=%d, random_timer=%u", iRandom, stTimerNode.uiRandom);
            return TIMER_NOT_EXIST;
        }

        return SUCCESS;
    }

    /**
     * @brief 校验内存中数据的有效性
     *
     * @returen 0-成功 其他-失败
     */
    int Verify()
    {
        return SUCCESS;
    }

    void ShowInfo()
    {
        printf("=========TimerPoolHeader=========\n");
        printf("TB_CUR_PT|%d|%d|%d|%d|%d\n\n", m_pstTPH->uiTB1CurPt, m_pstTPH->uiTB2CurPt, m_pstTPH->uiTB3CurPt, m_pstTPH->uiTB4CurPt, m_pstTPH->uiTB5CurPt);
        m_objTimerNodePool.Show();
    }

};

}

#endif
