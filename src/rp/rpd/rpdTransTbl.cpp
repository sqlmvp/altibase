/** 
 *  Copyright (c) 1999~2017, Altibase Corp. and/or its affiliates. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License, version 3,
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
 

/***********************************************************************
 * $Id: rpdTransTbl.cpp 82075 2018-01-17 06:39:52Z jina.kim $
 **********************************************************************/

#include <idl.h>
#include <ideErrorMgr.h>
#include <idErrorCode.h>
#include <smDef.h>
#include <smiMisc.h>
#include <smErrorCode.h>

#include <qci.h>
#include <rp.h>
#include <rpDef.h>
#include <rpdTransTbl.h>
#include <rpError.h>

#include <rpcManager.h>

rpdTransTbl::rpdTransTbl()
{
}

rpdTransTbl::~rpdTransTbl()
{
}

/***********************************************************************
 * Description : Replication Transaction Table�ʱ�ȭ
 *
 * aFlag - [IN] �� Table�� Replication Receiver�� ����ϸ�
 *              RPD_TRANSTBL_USE_RECEIVER,
 *              �ƴϸ�
 *              RPD_TRANSTBL_USE_SENDER
 *
 **********************************************************************/
IDE_RC rpdTransTbl::initialize(SInt aFlag, UInt aTransactionPoolSize)
{
    UInt   i;
    idBool sSvpPoolInit = ID_FALSE;
    idBool sInitTransPool = ID_FALSE;
    UInt   sTransactionPoolSize = 0;

    mTransTbl    = NULL;
    mTblSize     = smiGetTransTblSize();
    mFlag        = aFlag;
    (void)idCore::acpAtomicSet32( &mATransCnt, 0);
    mLFGCount    = 1; //[TASK-6757]LFG,SN ����
    mEndSN       = SM_SN_NULL;
    mOverAllocTransCount = 0;

    if ( aTransactionPoolSize > mTblSize )
    {
        sTransactionPoolSize = mTblSize;
    }
    else
    {
        sTransactionPoolSize = aTransactionPoolSize;
    }
    // BUG-28206 ���ʿ��� Transaction Begin�� ����
    IDE_TEST(mSvpPool.initialize(IDU_MEM_RP_RPD,
                                 (SChar *)"RP_SAVEPOINT_POOL",
                                 1,
                                 ID_SIZEOF(rpdSavepointEntry),
                                 mTblSize * RP_INIT_SAVEPOINT_COUNT_PER_TX,
                                 IDU_AUTOFREE_CHUNK_LIMIT, //chunk max(default)
                                 ID_FALSE,                 //use mutex(no use)
                                 8,                        //align byte(default)
                                 ID_FALSE,				   //ForcePooling
                                 ID_TRUE,				   //GarbageCollection
                                 ID_TRUE)                  //HWCacheLine
             != IDE_SUCCESS);
    sSvpPoolInit = ID_TRUE;

    IDU_FIT_POINT_RAISE( "rpdTransTbl::initialize::malloc::TransTbl",
                          ERR_MEMORY_ALLOC_TRANS_TABLE );
    IDE_TEST_RAISE(iduMemMgr::malloc(IDU_MEM_RP_RPD,
                                     mTblSize * ID_SIZEOF(rpdTransTblNode),
                                     (void**)&mTransTbl,
                                     IDU_MEM_IMMEDIATE)
                   != IDE_SUCCESS, ERR_MEMORY_ALLOC_TRANS_TABLE);

    /* BUG-18045 [Eager Mode] ���� Tx Slot�� ����ϴ� ���� Tx�� ������ �����Ѵ�. */
    IDE_TEST_RAISE(mAbortTxMutex.initialize((SChar *)"Repl_Tx_Table_Mtx",
                                            IDU_MUTEX_KIND_NATIVE,
                                            IDV_WAIT_INDEX_NULL)
                   != IDE_SUCCESS, ERR_MUTEX_INIT);

    if ( mFlag == RPD_TRANSTBL_USE_RECEIVER )
    {
        IDE_TEST( initTransPool() != IDE_SUCCESS );
        sInitTransPool = ID_TRUE;

        for( i = 0; i < mTblSize; i++ )
        {
            mTransTbl[i].mLogAnlz = NULL;
            initTransNode(mTransTbl + i);

            if ( i < sTransactionPoolSize )
            {
                IDE_TEST( allocTransNode() != IDE_SUCCESS );
            }
        }
    }
    else /*aFlag == RPD_TRANSTBL_USE_SENDER */
    {
        for( i = 0; i < mTblSize; i++ )
        {
            mTransTbl[i].mLogAnlz = NULL;
            initTransNode( mTransTbl + i );
        }
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION(ERR_MEMORY_ALLOC_TRANS_TABLE);
    {
        IDE_ERRLOG(IDE_RP_0);
        IDE_SET(ideSetErrorCode(rpERR_ABORT_MEMORY_ALLOC,
                                "rpdTransTbl::initialize",
                                "mTransTbl"));
    }
    IDE_EXCEPTION(ERR_MUTEX_INIT);
    {
        IDE_ERRLOG(IDE_RP_0);
        IDE_SET(ideSetErrorCode(rpERR_FATAL_ThrMutexInit));
        IDE_ERRLOG(IDE_RP_0);

        IDE_CALLBACK_FATAL("[Repl] Mutex initialization error");
    }
    IDE_EXCEPTION_END;
    IDE_ERRLOG(IDE_RP_0);
    IDE_PUSH();

    if ( ( sInitTransPool == ID_TRUE ) && 
         ( mFlag == RPD_TRANSTBL_USE_RECEIVER ) )
    {
        destroyTransPool();
    }
    
    if(mTransTbl != NULL)
    {
        (void)iduMemMgr::free(mTransTbl);
        mTransTbl = NULL;
    }

    if(sSvpPoolInit == ID_TRUE)
    {
        (void)mSvpPool.destroy(ID_FALSE);
    }

    IDE_POP();
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Replication Transaction Table�Ҹ���
 *
 **********************************************************************/
void rpdTransTbl::destroy()
{
    UInt i;

    // BUG-28206 ���ʿ��� Transaction Begin�� ����
    if(mSvpPool.destroy(ID_FALSE) != IDE_SUCCESS)
    {
        IDE_ERRLOG(IDE_RP_0);
    }

    if ( isReceiver() == ID_TRUE )
    {
        destroyTransPool();
    }
    else
    {
        /* nothing to do */
    }

    for(i = 0; i < mTblSize; i++)
    {
        if(mTransTbl[i].mLogAnlz != NULL)
        {
            (void)iduMemMgr::free(mTransTbl[i].mLogAnlz);
            mTransTbl[i].mLogAnlz = NULL;
        }
    }

    /* Transaction Table�� �Ҵ�� ��� Memory�� �����Ѵ�.*/
    (void)iduMemMgr::free(mTransTbl);
    mTransTbl = NULL;

    if(mAbortTxMutex.destroy() != IDE_SUCCESS)
    {
        IDE_ERRLOG(IDE_RP_0);
    }

    IDE_ASSERT( idCore::acpAtomicGet32( &mATransCnt ) == 0);

    return;
}

/***********************************************************************
 * Description : rpdTransTblNode �ʱ�ȭ
 *
 * aTransNode  - [IN] �ʱ�ȭ�� ������ rpdTransTblNode
 *
 **********************************************************************/
void rpdTransTbl::initTransNode(rpdTransTblNode *aTransNode)
{
    aTransNode->mRepName = NULL;
    aTransNode->mRemoteTID = ID_UINT_MAX;
    aTransNode->mMyTID = ID_UINT_MAX;
    aTransNode->mBeginFlag = ID_FALSE;
    aTransNode->mAbortSN = SM_SN_NULL;
    aTransNode->mTxListIdx = ID_UINT_MAX;
    aTransNode->mIsConflict = ID_FALSE;

    /* BUG-21858 DML + LOB ó�� */
    aTransNode->mSkipLobLogFlag = ID_FALSE;

    //BUG-24398
    aTransNode->mSendLobLogFlag = ID_FALSE;

    /* BUG-18045 [Eager Mode] ���� Tx Slot�� ����ϴ� ���� Tx�� ������ �����Ѵ�. */
    IDE_ASSERT(mAbortTxMutex.lock( NULL /* idvSQL* */) == IDE_SUCCESS);
    aTransNode->mBeginSN = SM_SN_NULL;
    aTransNode->mAbortFlag = ID_FALSE;
    IDE_ASSERT(mAbortTxMutex.unlock() == IDE_SUCCESS);

    aTransNode->mLocHead.mNext = NULL;
    aTransNode->mSNMapEntry    = NULL;

    /* PROJ-1442 Replication Online �� DDL ���
     * DDL Transaction ���� �ʱ�ȭ
     */
    aTransNode->mIsDDLTrans = ID_FALSE;
    IDU_LIST_INIT(&aTransNode->mItemMetaList);

    // BUG-28206 ���ʿ��� Transaction Begin�� ����
    IDU_LIST_INIT(&aTransNode->mSvpList);
    aTransNode->mPSMSvp        = NULL;
    aTransNode->mIsSvpListSent = ID_FALSE;
    aTransNode->mTrans.mTransForConflictResolution = NULL;
    aTransNode->mTrans.mSetPSMSavepoint = ID_FALSE;
}

/***********************************************************************
 * Description : aRemoteTID�� �ش��ϴ� Transaction Slot�� �Ҵ��ϰ� �ʱ�ȭ�Ѵ�.
 *
 * aRemoteTID - [IN]  Transaction ID
 * aBeginSN   - [IN]  Transaction �� Begin SN
 *
 **********************************************************************/
IDE_RC rpdTransTbl::insertTrans(iduMemAllocator * aAllocator,
                                smTID       aRemoteTID,
                                smSN        aBeginSN,
                                iduMemPool *aChainedValuePool)
{
    UInt          sIndex;
    rpdTrans    * sTrans = NULL;

    // BUG-17548
    IDE_ASSERT(aBeginSN != SM_SN_NULL);

    /* aRemoteTID�� ����� Transaction Slot�� ã�´�.*/
    sIndex = aRemoteTID % mTblSize;

    /* ���ÿ� ���� Transaction Table Slot�� ����ϴ��� �˻��Ѵ�. */
    IDE_TEST_RAISE(isATransNode(&(mTransTbl[sIndex])) == ID_TRUE,
                   ERR_ALREADY_BEGIN);

    /* TID�� Setting�Ѵ� */
    mTransTbl[sIndex].mRemoteTID = aRemoteTID;

    /* BUG-18045 [Eager Mode] ���� Tx Slot�� ����ϴ� ���� Tx�� ������ �����Ѵ�. */
    IDE_ASSERT(mAbortTxMutex.lock( NULL /* idvSQL* */) == IDE_SUCCESS);
    mTransTbl[sIndex].mBeginSN = aBeginSN;
    IDE_ASSERT(mAbortTxMutex.unlock() == IDE_SUCCESS);

    if ( isReceiver() == ID_TRUE )
    {
        IDE_TEST_RAISE( getTransNode( &sTrans ) != IDE_SUCCESS, ERR_GET_TRANS );
        mTransTbl[sIndex].mTrans.mRpdTrans = sTrans;
    }
    else
    {
        if(mTransTbl[sIndex].mLogAnlz == NULL)
        {
            IDU_FIT_POINT_RAISE( "rpdTransTbl::insertTrans::malloc::LogAnalyzer",
                                 ERR_MEMORY_ALLOC_LOG_ANALYZER,
                                 rpERR_ABORT_MEMORY_ALLOC,
                                 "rpdTransTbl::insertTrans",
                                 "mTransTbl[sIndex].mLogAnlz" );
            IDE_TEST_RAISE(iduMemMgr::malloc(IDU_MEM_RP_RPD,
                                             ID_SIZEOF(rpdLogAnalyzer),
                                             (void**)&mTransTbl[sIndex].mLogAnlz,
                                             IDU_MEM_IMMEDIATE)
                           != IDE_SUCCESS, ERR_MEMORY_ALLOC_LOG_ANALYZER);
            new (mTransTbl[sIndex].mLogAnlz) rpdLogAnalyzer;

            IDE_TEST(mTransTbl[sIndex].mLogAnlz->initialize(aAllocator, aChainedValuePool)
                     != IDE_SUCCESS);
        }
        else
        {
            //nothing to do 
        }
    }

    /* ���ο� Transaction�� Begin�Ǿ����Ƿ� Active Trans Count��
       �÷��ش�.*/
    (void)idCore::acpAtomicInc32( &mATransCnt );

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_ALREADY_BEGIN);
    {
        IDE_SET(ideSetErrorCode(rpERR_ABORT_RP_ENTRY_EXIST));

        ideLog::log(IDE_RP_0, RP_TRC_TT_DUPLICATED_BEGIN,
                    mTransTbl[sIndex].mBeginSN,
                    aBeginSN,
                    aRemoteTID,
                    mTblSize);

        IDE_DASSERT(0);
    }
    IDE_EXCEPTION(ERR_MEMORY_ALLOC_LOG_ANALYZER);
    {
        IDE_ERRLOG(IDE_RP_0);
        IDE_SET(ideSetErrorCode(rpERR_ABORT_MEMORY_ALLOC,
                                "rpdTransTbl::insertTrans",
                                "mTransTbl[sIndex].mLogAnlz"));

        mTransTbl[sIndex].mLogAnlz = NULL;
        initTransNode( &(mTransTbl[sIndex]) );
    }
    IDE_EXCEPTION(ERR_GET_TRANS);
    {
        initTransNode( &(mTransTbl[sIndex]) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : aRemoteTID�� �ش��ϴ� Transaction Slot�� ��ȯ�ϰ� ������
 *               member�� ���ؼ� �Ҹ��ڸ� ȣ���Ѵ�.
 *
 * aRemoteTID - [IN]  end��ų Transaction ID
 *
 **********************************************************************/
void rpdTransTbl::removeTrans(smTID  aRemoteTID)
{
    iduListNode       * sNode;
    iduListNode       * sDummy;
    rpdItemMetaEntry  * sItemMetaEntry;
    rpdSavepointEntry * sSavepointEntry;
    UInt                sIndex;
    rpdTransTblNode   * sTransTblNode = NULL;

    sIndex = aRemoteTID % mTblSize;

    sTransTblNode = &(mTransTbl[sIndex]);

    if ( isATransNode( sTransTblNode ) == ID_TRUE )
    {
        if(isReceiver() == ID_TRUE)
        {
            /*return transaction to transaction pool*/
            if ( sTransTblNode->mTrans.mTransForConflictResolution != NULL )
            {
                removeTransNode( sTransTblNode->mTrans.mTransForConflictResolution );
            }
            else
            {
                /* Nothing to do */
            }
            removeTransNode( sTransTblNode->mTrans.mRpdTrans );
        }
        else
        {
            if ( sTransTblNode->mLogAnlz != NULL )
            {
                sTransTblNode->mLogAnlz->destroy();
            }
            else
            {
                /* Nothing to do */
            }

            /* PROJ-1442 Replication Online �� DDL ���
             * DDL Transaction ���� ����
             */
            IDU_LIST_ITERATE_SAFE( &sTransTblNode->mItemMetaList, sNode, sDummy )
            {
                sItemMetaEntry = (rpdItemMetaEntry *)sNode->mObj;
                IDU_LIST_REMOVE(sNode);

                (void)iduMemMgr::free(sItemMetaEntry->mLogBody);
                (void)iduMemMgr::free(sItemMetaEntry);
            }
        }

        /* BUG-28206 ���ʿ��� Transaction Begin�� ���� (Sender) */
        /* BUG-18028 Eager Mode���� Partial Rollback ���� (Receiver) */
        IDU_LIST_ITERATE_SAFE( &sTransTblNode->mSvpList, sNode, sDummy )
        {
            sSavepointEntry = (rpdSavepointEntry *)sNode->mObj;
            IDU_LIST_REMOVE(sNode);

            (void)mSvpPool.memfree(sSavepointEntry);
        }

        initTransNode( sTransTblNode );

        /* Transaction�� end�Ǿ����Ƿ� Active Trans Count�� �ٿ��ش�. */
        (void)idCore::acpAtomicDec32( &mATransCnt );
    }

    return;
}

/***********************************************************************
 * Description : Active Transaction�߿��� First Write Log�� SN�߿��� ����
 *               ���� SN���� �����´�.
 *
 * aSN - [OUT]  ���� ���� SN���� Return�� ����
 *
 **********************************************************************/
void rpdTransTbl::getMinTransFirstSN( smSN * aSN )
{
    UInt    i;
    smSN    sMinFirstSN = SM_SN_NULL;

    IDE_ASSERT( aSN != NULL );

    if ( idCore::acpAtomicGet32( &mATransCnt ) > 0 )
    {
        for(i = 0; i < mTblSize; i++)
        {
            if(isATransNode(mTransTbl + i) == ID_FALSE)
            {
                continue;
            }
            else
            {
                /* Active Transaction�� mBeginSN�� SM_SN_NULL�� �ƴϴ�.
                 * sender�� mBeginSN�� set�ϰ� sender�� �����Ƿ�
                 * mBeginSN�� ���� �� lock�� �� ��Ƶ� ��
                 */
                if(sMinFirstSN > mTransTbl[i].mBeginSN)
                {
                    sMinFirstSN = mTransTbl[i].mBeginSN;
                }
            }
        }
    }
    else
    {
        /* Nothing to do */
    }

    *aSN = sMinFirstSN;
}

/***********************************************************************
 * Description : ��� Active Transaction�� Rollback��Ű�� �Ҵ�� Slot�� ��ȯ
 *               �Ѵ�.
 *
 *
 **********************************************************************/
void rpdTransTbl::rollbackAllATrans()
{
    UInt i;
    smLobLocator     sRemoteLL = SMI_NULL_LOCATOR;
    smLobLocator     sLocalLL = SMI_NULL_LOCATOR;
    idBool           sIsExist;

    if(isReceiver() == ID_TRUE)
    {
        for(i = 0; i < mTblSize; i++)
        {
            if(isATransNode(mTransTbl + i) == ID_TRUE)
            {
                /* getFirstLocator,removeLocator�� TID�� Ʈ�������
                 * Active�� �ƴ� ���� ������ ��ȯ�ϱ� ������ �Ʒ���
                 *  if �����ȿ����� ������ ��ȯ���� �ʴ´�.
                 */
                IDE_ASSERT(getFirstLocator(mTransTbl[i].mRemoteTID,
                                           &sRemoteLL,
                                           &sLocalLL,
                                           &sIsExist)
                           == IDE_SUCCESS);

                while(sIsExist == ID_TRUE)
                {
                    removeLocator(mTransTbl[i].mRemoteTID, sRemoteLL);

                    if ( smiLob::closeLobCursor(sLocalLL) != IDE_SUCCESS )
                    {
                        IDE_WARNING( IDE_RP_0, RP_TRC_RA_ERR_LOB_CLOSE );
                    }
                    else
                    {
                        /* do noting */
                    }

                    IDE_ASSERT(getFirstLocator(mTransTbl[i].mRemoteTID,
                                                &sRemoteLL,
                                                &sLocalLL,
                                                &sIsExist)
                               == IDE_SUCCESS);
                }

                if ( mTransTbl[i].mTrans.rollback( mTransTbl[i].mRemoteTID ) != IDE_SUCCESS )
                {
                    IDE_ERRLOG(IDE_RP_0);
                    IDE_CALLBACK_FATAL("[Repl] Transaction Rollback Failure");
                }
                removeTrans(mTransTbl[i].mRemoteTID);
            }
        }
    }
    else
    {
        /* Sender�� ���� Transaction�� begin���� �ʰ�
           ������ �����ϰ� �ֱ� ������ Rollback�� �ʿ䰡
           ����.*/
        for( i = 0; i < mTblSize; i++ )
        {
            if( isATransNode( mTransTbl + i ) == ID_TRUE )
            {
                removeTrans(mTransTbl[i].mRemoteTID);
            }

            if( mTransTbl[i].mLogAnlz != NULL )
            {
                (void)iduMemMgr::free( mTransTbl[i].mLogAnlz );
                mTransTbl[i].mLogAnlz = NULL;
            }
            else
            {
                /* nothing to do */
            }
        }
    }

    return;
}

/***********************************************************************
 * Description : aTID Ʈ����� Entry�� Remote LOB Locator�� Local LOB Locator
 *               �� pair�� �����Ѵ�.
 *
 * aTID      - [IN]  Ʈ����� ID
 * aRemoteLL - [IN]  ������ Remote LOB Locator(Sender���� Locator)
 * aLocalLL  - [IN]  ������ Local LOB Locator(Reciever���� open�� Locator)
 *
 * Return
 *   IDE_SUCCESS - ����
 *   IDE_FAILURE
 *        - NOT Active : ���� Ʈ������� ��Ȱ�� ������
 *
 **********************************************************************/
IDE_RC rpdTransTbl::insertLocator(smTID aTID, smLobLocator aRemoteLL, smLobLocator aLocalLL)
{
    rpdTransTblNode *sTrNode   = NULL;
    rpdLOBLocEntry  *sLocEntry = NULL;

    IDE_TEST_RAISE(isATrans(aTID) != ID_TRUE, ERR_NOT_ACTIVE);
    sTrNode = getTrNode(aTID);

    IDE_TEST_RAISE(iduMemMgr::malloc(IDU_MEM_RP_RPD,
                                     ID_SIZEOF(rpdLOBLocEntry),
                                     (void **)&sLocEntry,
                                     IDU_MEM_IMMEDIATE)
                   != IDE_SUCCESS, ERR_MEMORY_ALLOC_LOC_ENTRY);

    sLocEntry->mRemoteLL    = aRemoteLL;
    sLocEntry->mLocalLL     = aLocalLL;
    sLocEntry->mNext        = sTrNode->mLocHead.mNext;
    sTrNode->mLocHead.mNext = sLocEntry;

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NOT_ACTIVE);
    {
        // Ʈ������� ���� ��Ȱ�� ������
        IDE_ERRLOG( IDE_RP_0 );
        IDE_SET( ideSetErrorCode( rpERR_ABORT_TRANSACTION_NOT_ACTIVE ) );
    }
    IDE_EXCEPTION(ERR_MEMORY_ALLOC_LOC_ENTRY);
    {
        IDE_ERRLOG(IDE_RP_0);
        IDE_SET(ideSetErrorCode(rpERR_ABORT_MEMORY_ALLOC,
                                "rpdTransTbl::insertLocator",
                                "sLocEntry"));
    }
    IDE_EXCEPTION_END;
    IDE_PUSH();

    if(sLocEntry != NULL)
    {
        (void)iduMemMgr::free(sLocEntry);
    }

    IDE_POP();
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Remote LOB Locator�� Key�� Ʈ����� Entry����
 *               Locator entry�� �����Ѵ�.
 *
 * aTID      - [IN]  Ʈ����� ID
 * aRemoteLL - [IN]  ������ Remote LOB Locator(Sender���� Locator)
 *
 **********************************************************************/
void rpdTransTbl::removeLocator(smTID aTID, smLobLocator aRemoteLL)
{
    rpdTransTblNode *sTrNode;
    rpdLOBLocEntry  *sPrevEntry;
    rpdLOBLocEntry  *sCurEntry;

    if(isATrans(aTID) == ID_TRUE)
    {
        sTrNode = getTrNode(aTID);

        sPrevEntry = &sTrNode->mLocHead;
        sCurEntry = sPrevEntry->mNext;

        while(sCurEntry != NULL)
        {
            if(sCurEntry->mRemoteLL == aRemoteLL)
            {
                sPrevEntry->mNext = sCurEntry->mNext;
                (void)iduMemMgr::free(sCurEntry);
                break;
            }
            sPrevEntry = sCurEntry;
            sCurEntry = sCurEntry->mNext;
        }
    }

    return;
}

/***********************************************************************
 * Description : aTID Ʈ����� Entry���� Remote LOB Locator�� Key��
 *               Local LOB Locator�� ��ȯ�Ѵ�.
 *
 * aTID      - [IN]  Ʈ����� ID
 * aRemoteLL - [IN]  ã�� Remote LOB Locator(Sender���� Locator)
 * aLocalLL  - [OUT] ��ȯ�� Local LOB Locator ���� ����(Reciever���� open�� Locator)
 *
 * Return
 *   IDE_SUCCESS - ����
 *   IDE_FAILURE
 *        - NOT Active : ���� Ʈ������� ��Ȱ�� ������
 *
 **********************************************************************/
IDE_RC rpdTransTbl::searchLocator( smTID          aTID,
                                   smLobLocator   aRemoteLL,
                                   smLobLocator  *aLocalLL,
                                   idBool        *aIsFound )
{
    rpdTransTblNode *sTrNode;
    rpdLOBLocEntry  *sCurEntry;

    *aIsFound = ID_FALSE;

    IDE_TEST_RAISE(isATrans(aTID) != ID_TRUE, ERR_NOT_ACTIVE);
    sTrNode = getTrNode(aTID);

    sCurEntry = sTrNode->mLocHead.mNext;

    while(sCurEntry != NULL)
    {
        if(sCurEntry->mRemoteLL == aRemoteLL)
        {
            *aLocalLL = sCurEntry->mLocalLL;
            *aIsFound = ID_TRUE;
            break;
        }
        sCurEntry = sCurEntry->mNext;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NOT_ACTIVE);
    {
        // Ʈ������� ���� ��Ȱ�� ������
        IDE_ERRLOG( IDE_RP_0 );
        IDE_SET( ideSetErrorCode( rpERR_ABORT_TRANSACTION_NOT_ACTIVE ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : aTID Ʈ����� Entry�� Locator List���� ù��° entry��
 *               Local LOB Locator�� ��ȯ�Ѵ�.
 *
 * aTID      - [IN]  Ʈ����� ID
 * aRemoteLL - [OUT] ��ȯ�� Remote LOB Locator ���� ����(Sender�� Locator)
 * aLocalLL  - [OUT] ��ȯ�� Local LOB Locator ���� ����(Reciever���� open�� Locator)
 *
 * Return
 *   IDE_SUCCESS - ����
 *   IDE_FAILURE
 *        - NOT Active : ���� Ʈ������� ��Ȱ�� ������
 *
 **********************************************************************/
IDE_RC rpdTransTbl::getFirstLocator( smTID          aTID,
                                     smLobLocator  *aRemoteLL,
                                     smLobLocator  *aLocalLL,
                                     idBool        *aIsExist )
{
    rpdTransTblNode *sTrNode;
    rpdLOBLocEntry  *sCurEntry;

    *aIsExist = ID_FALSE;

    IDE_TEST_RAISE(isATrans(aTID) != ID_TRUE, ERR_NOT_ACTIVE);
    sTrNode = getTrNode(aTID);

    sCurEntry = sTrNode->mLocHead.mNext;

    if(sCurEntry != NULL)
    {
        *aRemoteLL = sCurEntry->mRemoteLL;
        *aLocalLL = sCurEntry->mLocalLL;
        *aIsExist = ID_TRUE;
    }
    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NOT_ACTIVE);
    {
        // Ʈ������� ���� ��Ȱ�� ������
        IDE_ERRLOG( IDE_RP_0 );
        IDE_SET( ideSetErrorCode( rpERR_ABORT_TRANSACTION_NOT_ACTIVE ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*******************************************************************************
 * Description : aTID Ʈ����� Entry�� Item Meta List�� Entry�� �߰��Ѵ�.
 *
 * aTID                 - [IN]  Ʈ����� ID
 * aItemMeta            - [IN]  �߰��� Item Meta
 * aItemMetaLogBody     - [IN]  �߰��� Item Meta Body
 * aItemMetaLogBodySize - [IN]  �߰��� Item Meta Body�� ũ��
 ******************************************************************************/
IDE_RC rpdTransTbl::addItemMetaEntry(smTID          aTID,
                                     smiTableMeta * aItemMeta,
                                     const void   * aItemMetaLogBody,
                                     UInt           aItemMetaLogBodySize)
{
    rpdItemMetaEntry * sItemMetaEntry = NULL;
    UInt               sIndex         = aTID % mTblSize;

    IDU_FIT_POINT_RAISE( "rpdTransTbl::addItemMetaEntry::malloc::ItemMetaEntry",
                         ERR_MEMORY_ALLOC_ITEM_META_ENTRY );
    IDE_TEST_RAISE(iduMemMgr::malloc(IDU_MEM_RP_RPD,
                                     ID_SIZEOF(rpdItemMetaEntry),
                                     (void **)&sItemMetaEntry,
                                     IDU_MEM_IMMEDIATE)
                   != IDE_SUCCESS, ERR_MEMORY_ALLOC_ITEM_META_ENTRY);
    idlOS::memset((void *)sItemMetaEntry, 0, ID_SIZEOF(rpdItemMetaEntry));

    idlOS::memcpy((void *)&sItemMetaEntry->mItemMeta,
                  (const void  *)aItemMeta,
                  ID_SIZEOF(smiTableMeta));

    IDU_FIT_POINT_RAISE( "rpdTransTbl::addItemMetaEntry::malloc::LogBody",
                         ERR_MEMORY_ALLOC_LOG_BODY );
    IDE_TEST_RAISE(iduMemMgr::malloc(IDU_MEM_RP_RPD,
                                     (ULong)aItemMetaLogBodySize,
                                     (void **)&sItemMetaEntry->mLogBody,
                                     IDU_MEM_IMMEDIATE)
                   != IDE_SUCCESS, ERR_MEMORY_ALLOC_LOG_BODY);
    idlOS::memcpy((void *)sItemMetaEntry->mLogBody,
                   aItemMetaLogBody,
                   aItemMetaLogBodySize);

    // ���Ŀ��� �������� �ʴ´�
    IDU_LIST_INIT_OBJ(&(sItemMetaEntry->mNode), sItemMetaEntry);
    IDU_LIST_ADD_LAST(&mTransTbl[sIndex].mItemMetaList,
                      &(sItemMetaEntry->mNode));

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_MEMORY_ALLOC_ITEM_META_ENTRY);
    {
        IDE_ERRLOG(IDE_RP_0);
        IDE_SET(ideSetErrorCode(rpERR_ABORT_MEMORY_ALLOC,
                                "rpdTransTbl::addItemMetaEntry",
                                "sItemMetaEntry"));
    }
    IDE_EXCEPTION(ERR_MEMORY_ALLOC_LOG_BODY);
    {
        IDE_ERRLOG(IDE_RP_0);
        IDE_SET(ideSetErrorCode(rpERR_ABORT_MEMORY_ALLOC,
                                "rpdTransTbl::addItemMetaEntry",
                                "sItemMetaEntry->mLogBody"));
    }
    IDE_EXCEPTION_END;
    IDE_PUSH();

    if(sItemMetaEntry != NULL)
    {
        if(sItemMetaEntry->mLogBody != NULL)
        {
            (void)iduMemMgr::free(sItemMetaEntry->mLogBody);
        }

        (void)iduMemMgr::free(sItemMetaEntry);
    }

    IDE_POP();
    return IDE_FAILURE;
}

/*******************************************************************************
 * Description : aTID Ʈ����� Entry�� Item Meta List���� ù ��° Entry�� ��ȯ�Ѵ�.
 *
 * aTID           - [IN]  Ʈ����� ID
 * aItemMetaEntry - [OUT] ��ȯ�� ù ��° Item Meta Entry
 ******************************************************************************/
void rpdTransTbl::getFirstItemMetaEntry(smTID               aTID,
                                        rpdItemMetaEntry ** aItemMetaEntry)
{
    iduListNode * sNode;
    iduListNode * sDummy;
    UInt          sIndex = aTID % mTblSize;

    *aItemMetaEntry = NULL;

    if(IDU_LIST_IS_EMPTY(&mTransTbl[sIndex].mItemMetaList) != ID_TRUE)
    {
        IDU_LIST_ITERATE_SAFE(&mTransTbl[sIndex].mItemMetaList, sNode, sDummy)
        {
            *aItemMetaEntry = (rpdItemMetaEntry *)sNode->mObj;
            break;
        }
    }

    IDE_ASSERT(*aItemMetaEntry != NULL);

    return;
}

/*******************************************************************************
 * Description : aTID Ʈ����� Entry�� Item Meta List���� ù ��° Entry�� �����Ѵ�.
 *
 * aTID           - [IN]  Ʈ����� ID
 ******************************************************************************/
void rpdTransTbl::removeFirstItemMetaEntry(smTID aTID)
{
    iduListNode      * sNode;
    iduListNode      * sDummy;
    UInt               sIndex = aTID % mTblSize;
    rpdItemMetaEntry * sItemMetaEntry = NULL;

    if(IDU_LIST_IS_EMPTY(&mTransTbl[sIndex].mItemMetaList) != ID_TRUE)
    {
        IDU_LIST_ITERATE_SAFE(&mTransTbl[sIndex].mItemMetaList, sNode, sDummy)
        {
            sItemMetaEntry = (rpdItemMetaEntry *)sNode->mObj;
            IDU_LIST_REMOVE(sNode);

            (void)iduMemMgr::free((void *)sItemMetaEntry->mLogBody);
            (void)iduMemMgr::free((void *)sItemMetaEntry);
            break;
        }
    }

    return;
}

/*******************************************************************************
 * Description : aTID Ʈ����� Entry�� Item Meta List�� Entry�� �ִ��� Ȯ���Ѵ�.
 *
 * aTID         - [IN]  Ʈ����� ID
 ******************************************************************************/
idBool rpdTransTbl::existItemMeta(smTID aTID)
{
    UInt sIndex = aTID % mTblSize;

    return (IDU_LIST_IS_EMPTY(&mTransTbl[sIndex].mItemMetaList) != ID_TRUE)
           ? ID_TRUE : ID_FALSE;
}

/*******************************************************************************
 * Description : Savepoint Entry List�� �������� Entry�� �߰��Ѵ�.
 *
 * aTID              - [IN]  Ʈ����� ID
 * aSN               - [IN]  �߰��� Savepoint SN
 * aType             - [IN]  �߰��� Savepoint Type
 * aSvpName          - [IN]  �߰��� Savepoint Name
 * aImplicitSvpDepth - [IN]  Implicit Savepoint�� ����� Depth
 ******************************************************************************/
IDE_RC rpdTransTbl::addLastSvpEntry(smTID            aTID,
                                    smSN             aSN,
                                    rpSavepointType  aType,
                                    SChar           *aSvpName,
                                    UInt             aImplicitSvpDepth)
{
    rpdTransTblNode   *sTransNode        = &mTransTbl[aTID % mTblSize];
    rpdSavepointEntry *sSvpEntry         = NULL;
    idBool             sNeedFreeSvpEntry = ID_FALSE;
    UInt               sSvpNameLen       = 0;

    IDE_ASSERT(aSvpName != NULL);

    IDU_FIT_POINT( "rpdTransTbl::addLastSvpEntry::alloc::SvpEntry" );
    IDE_TEST(mSvpPool.alloc((void**)&sSvpEntry) != IDE_SUCCESS);
    sNeedFreeSvpEntry = ID_TRUE;

    sSvpEntry->mSN    = aSN;
    sSvpEntry->mType  = aType;

    sSvpNameLen = idlOS::strlen( aSvpName );

    IDE_DASSERT( sSvpNameLen <= RP_SAVEPOINT_NAME_LEN );

    idlOS::memcpy( (void *)sSvpEntry->mName,
                   (const void  *)aSvpName,
                   sSvpNameLen + 1);

    // ����ȭ...
    switch(aType)
    {
        case RP_SAVEPOINT_IMPLICIT :
            IDE_ASSERT(aImplicitSvpDepth != SMI_STATEMENT_DEPTH_NULL);

            // Depth 1�� ���, ���� Implicit Savepoint�� ��� ����
            if(aImplicitSvpDepth == 1)
            {
                removeLastImplicitSvpEntries(&sTransNode->mSvpList);
            }
            break;

        case RP_SAVEPOINT_EXPLICIT :
            // ���� Implicit Savepoint�� ��� ����
            removeLastImplicitSvpEntries(&sTransNode->mSvpList);
            break;

        case RP_SAVEPOINT_PSM :
            // ���� Implicit Savepoint�� ��� ����
            removeLastImplicitSvpEntries(&sTransNode->mSvpList);

            // PSM Savepoint�� ������ 1���� ��ȿ�ϹǷ�, ������ PSM Savepoint�� ����
            if(sTransNode->mPSMSvp != NULL)
            {
                IDU_LIST_REMOVE(&sTransNode->mPSMSvp->mNode);
                (void)mSvpPool.memfree(sTransNode->mPSMSvp);
            }
            sTransNode->mPSMSvp = sSvpEntry;
            break;

        default :
            IDE_ASSERT(0);
    }

    IDU_LIST_INIT_OBJ(&(sSvpEntry->mNode), sSvpEntry);
    IDU_LIST_ADD_LAST(&sTransNode->mSvpList, &(sSvpEntry->mNode));
    sNeedFreeSvpEntry = ID_FALSE;

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    IDE_PUSH();

    if(sNeedFreeSvpEntry == ID_TRUE)
    {
        (void)mSvpPool.memfree(sSvpEntry);
    }

    IDE_POP();
    return IDE_FAILURE;
}

/*******************************************************************************
 * Description : Savepoint Entry List���� ù ��° Entry�� ��ȯ�Ѵ�.
 *
 * aTID            - [IN]  Ʈ����� ID
 * aSavepointEntry - [OUT] ��ȯ�� ù ��° Savepoint Entry
 ******************************************************************************/
void rpdTransTbl::getFirstSvpEntry(smTID                aTID,
                                   rpdSavepointEntry ** aSavepointEntry)
{
    iduListNode     *sNode;
    rpdTransTblNode *sTransNode = &mTransTbl[aTID % mTblSize];

    *aSavepointEntry = NULL;

    if(IDU_LIST_IS_EMPTY(&sTransNode->mSvpList) != ID_TRUE)
    {
        sNode = IDU_LIST_GET_FIRST(&sTransNode->mSvpList);
        *aSavepointEntry = (rpdSavepointEntry *)sNode->mObj;
    }

    return;
}

/*******************************************************************************
 * Description : Savepoint Entry List���� Savepoint Entry�� �����Ѵ�.
 *
 * aSavepointEntry - [IN]  ������ Savepoint Entry
 ******************************************************************************/
void rpdTransTbl::removeSvpEntry(rpdSavepointEntry * aSavepointEntry)
{
    IDU_LIST_REMOVE(&aSavepointEntry->mNode);
    (void)mSvpPool.memfree(aSavepointEntry);

    return;
}

/*******************************************************************************
 * Description : Savepoint Entry List�� ������ Implicit Savepoint���� �����Ѵ�.
 *
 * aSvpList   - [IN]  Savepoint Entry List
 ******************************************************************************/
void rpdTransTbl::removeLastImplicitSvpEntries(iduList *aSvpList)
{
    iduListNode       * sNode;
    iduListNode       * sDummy;
    rpdSavepointEntry * sSavepointEntry;

    IDU_LIST_ITERATE_BACK_SAFE(aSvpList, sNode, sDummy)
    {
        sSavepointEntry = (rpdSavepointEntry *)sNode->mObj;

        if(sSavepointEntry->mType != RP_SAVEPOINT_IMPLICIT)
        {
            break;
        }

        IDU_LIST_REMOVE(sNode);
        (void)mSvpPool.memfree(sSavepointEntry);
    }

    return;
}

/*******************************************************************************
 * Description : Savepoint Entry List�� Savepoint Abort�� �����Ѵ�.
 *               �߰�������, Partial Rollback�� ��ġ�� ��ȯ�Ѵ�.
 *
 * aTID       - [IN]  Ʈ����� ID
 * aSvpName   - [IN]  Savepoint Abort Name
 * aSN        - [OUT] Partial Rollback�� ��ġ
 ******************************************************************************/
void rpdTransTbl::applySvpAbort(smTID  aTID,
                                SChar *aSvpName,
                                smSN  *aSN)
{
    rpdTransTblNode   *sTransNode = &mTransTbl[aTID % mTblSize];
    iduListNode       *sNode;
    iduListNode       *sDummy;
    rpdSavepointEntry *sSavepointEntry;
    SChar             *sPsmSvpName = NULL;

    sPsmSvpName = smiGetPsmSvpName();
    *aSN        = SM_SN_NULL;

    /* process PSM Savepoint Abort */
    if(idlOS::strcmp(aSvpName, sPsmSvpName) == 0)
    {
        /* PSM Savepoint�� 0~1���� �����Ƿ�, List�� �������� �ʴ´�. */
        *aSN = sTransNode->mPSMSvp->mSN;
        IDE_CONT(SKIP_APPLY);
    }

    /* process Implicit/Explicit Savepoint Abort */
    IDU_LIST_ITERATE_BACK_SAFE(&sTransNode->mSvpList, sNode, sDummy)
    {
        sSavepointEntry = (rpdSavepointEntry *)sNode->mObj;

        if(idlOS::strcmp(sSavepointEntry->mName, aSvpName) == 0)
        {
            /* Current Savepoint's Name is Savepoint Abort's Name */
            *aSN = sSavepointEntry->mSN;
            break;
        }

        if(sSavepointEntry->mType == RP_SAVEPOINT_PSM)
        {
            /* Current Savepoint is PSM Savepoint Set
             *
             *  PSM()
             *  {
             *      SAVEPOINT SP1;
             *      INSERT 1;
             *      ROLLBACK TO SAVEPOINT SP1;
             *      INSERT 1;
             *  }
             *  �� �����ϸ�,
             *      SAVEPOINT SP1;
             *      PSM SAVEPOINT;
             *      INSERT 1; (with Implicit Savepoint depth 1)
             *      ROLLBACK TO SAVEPOINT SP1;  <- POINT 1
             *      INSERT 1; (with Implicit Savepoint depth 1)
             *      ROLLBACK TO IMPLICIT SAVEPOINT DEPTH 1;
             *      ROLLBACK TO PSM SAVEPOINT;  <- POINT 2
             *  �� ���� �α� ���ڵ尡 ��ϵȴ�.
             *  �׷���, POINT 1���� PSM Savepoint�� �����ߴٸ�, POINT 2���� �����Ѵ�.
             *  ����, PSM Savepoint�� �������� �ʴ´�.
             */
        }
        else
        {
            /* remove Current Savepoint from Savepoint List */
            IDU_LIST_REMOVE(sNode);
            (void)mSvpPool.memfree(sSavepointEntry);
        }
    }

    RP_LABEL(SKIP_APPLY);

    return;
}

IDE_RC rpdTransTbl::initTransPool()
{
    idBool sTransPoolInit = ID_FALSE;
    IDE_TEST(mTransPool.initialize(IDU_MEM_RP_RPD,
                                   (SChar *)"RP_TRANSACTION_POOL",
                                   1,
                                   ID_SIZEOF( rpdTrans ),
                                   mTblSize,
                                   IDU_AUTOFREE_CHUNK_LIMIT,//chunk max(default)
                                   ID_FALSE,                //use mutex(no use)
                                   8,                       //align byte(default)
                                   ID_FALSE,				//ForcePooling 
                                   ID_TRUE,					//GarbageCollection
                                   ID_TRUE)                 //HWCacheLine
             != IDE_SUCCESS);
    sTransPoolInit = ID_TRUE;
    
    IDU_LIST_INIT( &mFreeTransList );
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    if(sTransPoolInit == ID_TRUE)
    {
        (void)mTransPool.destroy(ID_FALSE);
    }
    else
    {
        /*nothing to do*/
    }
    return IDE_FAILURE;
}

IDE_RC rpdTransTbl::allocTransNode()
{
    rpdTrans *sTrans = NULL;

    IDU_FIT_POINT( "rpdTransTbl::allocTransNode::alloc::TransPool",
                   rpERR_ABORT_MEMORY_ALLOC,
                   "rpdTransTbl::allocTransNode",
                   "TransPool" );
    IDE_TEST( mTransPool.alloc( (void**)&sTrans ) != IDE_SUCCESS );
    IDE_TEST( sTrans->mSmiTrans.initialize() != IDE_SUCCESS );
    
    IDU_LIST_INIT_OBJ( &( sTrans->mNode ), sTrans );
    IDU_LIST_ADD_LAST( &mFreeTransList, &( sTrans->mNode ) );

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    if ( sTrans != NULL )
    {
        IDE_ERRLOG(IDE_RP_0);
        (void)mTransPool.memfree( sTrans );
        sTrans = NULL;
    }
    else
    {
        /*nothing to do*/
    }
    return IDE_FAILURE;
}

void rpdTransTbl::destroyTransPool()
{
    rpdTrans    * sTrans = NULL;
    iduListNode * sNode = NULL;
    iduListNode * sDummy = NULL;
    
    IDU_LIST_ITERATE_SAFE( &mFreeTransList, sNode, sDummy )
    {
        sTrans = (rpdTrans *) sNode->mObj;
        if ( sTrans->mSmiTrans.destroy( NULL ) != IDE_SUCCESS )
        {
            IDE_ERRLOG( IDE_RP_0 );
        }
        IDU_LIST_REMOVE( sNode );
        (void)mTransPool.memfree( sTrans );
    }
    
    if( mTransPool.destroy( ID_FALSE ) != IDE_SUCCESS )
    {
        IDE_ERRLOG( IDE_RP_0 );
    }
    else
    {
        /*nothing to do*/
    }

    return;
}

IDE_RC rpdTransTbl::getTransNode(rpdTrans** aRpdTrans)
{
    rpdTrans    * sTrans;
    iduListNode * sNode;

    idBool        sIsAlloced = ID_FALSE;

    /*get transaction from transaction pool*/
    if( IDU_LIST_IS_EMPTY( &mFreeTransList ) != ID_TRUE )
    {
        sNode = IDU_LIST_GET_FIRST( &mFreeTransList );
        sTrans = (rpdTrans *)sNode->mObj;
        IDU_LIST_REMOVE( sNode );
    }
    else /*transaction pool is empty*/
    {
        IDU_FIT_POINT( "rpdTransTbl::getTransNode::alloc::TransPool",
                       rpERR_ABORT_MEMORY_ALLOC,
                       "rpdTransTbl::getTransNode",
                       "TransPool" );
        IDE_TEST( mTransPool.alloc( (void**) &sTrans ) != IDE_SUCCESS );
        sIsAlloced = ID_TRUE;

        IDE_TEST( sTrans->mSmiTrans.initialize() != IDE_SUCCESS );
        IDU_LIST_INIT_OBJ( &( sTrans->mNode ), sTrans );
        mOverAllocTransCount++;
    }
    
    *aRpdTrans = sTrans;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    if ( sIsAlloced == ID_TRUE )
    {
        (void)mTransPool.memfree( sTrans );
    }
    else
    {
        /* do nothing */
    }

    IDE_POP();

    return IDE_FAILURE;
}

void rpdTransTbl::removeTransNode(rpdTrans* aRpdTrans)
{
    if ( mOverAllocTransCount > 0 )
    {
        if ( aRpdTrans->mSmiTrans.destroy( NULL ) != IDE_SUCCESS )
        {
            IDE_ERRLOG( IDE_RP_0 );
        }
        (void)mTransPool.memfree( aRpdTrans );
        mOverAllocTransCount--;
    }
    else
    {
        /*return transaction to transaction pool*/
        IDU_LIST_ADD_LAST( &mFreeTransList, &( aRpdTrans->mNode ) );
    }
    aRpdTrans = NULL;
}

IDE_RC rpdTransTbl::buildRecordForReplReceiverTransTbl( void                    * aHeader,
                                                        void                    * /* aDumpObj */,
                                                        iduFixedTableMemory     * aMemory,
                                                        SChar                   * aRepName,
                                                        UInt                      aParallelID,
                                                        SInt                      aParallelApplyIndex )
{
    UInt                      i = 0;
    UInt                      sTransTblSize = 0;
    rpdTransTblNode         * sTransTblNodeArray = NULL;
    rpdTransTblNodeInfo       sTransTblNodeInfo;

    sTransTblNodeArray = getTransTblNodeArray();

    IDE_TEST_CONT( sTransTblNodeArray == NULL, NORMAL_EXIT );

    sTransTblSize = getTransTblSize();
    for ( i = 0; i < sTransTblSize; i++ )
    {
        if ( isATransNode( &(sTransTblNodeArray[i]) ) == ID_TRUE )
        {
            sTransTblNodeArray[i].mMyTID = sTransTblNodeArray[i].mTrans.getTransID();

            //sTransTblNodeInfo ����
            sTransTblNodeInfo.mRepName    = aRepName;
            sTransTblNodeInfo.mParallelID = aParallelID;
            sTransTblNodeInfo.mParallelApplyIndex = aParallelApplyIndex;

            sTransTblNodeInfo.mMyTID      = sTransTblNodeArray[i].mMyTID;
            sTransTblNodeInfo.mRemoteTID  = sTransTblNodeArray[i].mRemoteTID;
            sTransTblNodeInfo.mBeginFlag  = sTransTblNodeArray[i].mBeginFlag;
            sTransTblNodeInfo.mBeginSN    = sTransTblNodeArray[i].mBeginSN;

            IDE_TEST( iduFixedTable::buildRecord( aHeader,
                                                  aMemory,
                                                  (void *) &(sTransTblNodeInfo) )
                      != IDE_SUCCESS );
        }
        else
        {
            /* do nothing */
        }
    }

    RP_LABEL( NORMAL_EXIT );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC rpdTransTbl::allocConflictResolutionTransNode( smTID aTID )
{
    rpdTrans * sRpdTrans = NULL;
    SInt sIndex = getTransSlotID( aTID );

    IDE_TEST( getTransNode( &sRpdTrans ) != IDE_SUCCESS );

    mTransTbl[sIndex].mTrans.mTransForConflictResolution = sRpdTrans;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
