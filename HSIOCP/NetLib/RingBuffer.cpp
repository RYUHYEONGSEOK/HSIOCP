#include "stdafx.h"
#include "RingBuffer.h"

namespace NetLib
{
	bool RingBuffer::Create(const int ringBufferSize)
	{
		if (ringBufferSize == INVALID_VALUE)
		{
			return false;
		}
		m_RingBufferSize = ringBufferSize;

		SAFE_DELETE_ARR(m_pRingBuffer);

		m_pRingBuffer = new char[m_RingBufferSize];

		m_pBeginMark = m_pRingBuffer;
		m_pEndMark = m_pBeginMark + m_RingBufferSize - 1;

		Init();
		return true;
	}

	void RingBuffer::Init(void)
	{
		SpinLock Lock(&m_CS);

		m_UsedBufferSize = 0;

		m_pCurMark = m_pBeginMark;
		m_pGettedBufferMark = m_pBeginMark;
		m_pLastMoveMark = m_pEndMark;
	}

	char* RingBuffer::GetBuffer(const int reqReadSize, OUT int& resReadSize)
	{
		SpinLock Lock(&m_CS);

		char* pResult = nullptr;
		if (m_pLastMoveMark == m_pGettedBufferMark)
		{
			m_pGettedBufferMark = m_pBeginMark;
			m_pLastMoveMark = m_pEndMark;
		}

		if (m_UsedBufferSize > reqReadSize)
		{
			if ((m_pLastMoveMark - m_pGettedBufferMark) >= reqReadSize)
			{
				resReadSize = reqReadSize;
			}
			else
			{
				resReadSize = static_cast<int>(m_pLastMoveMark - m_pGettedBufferMark);
			}

			pResult = m_pGettedBufferMark;
			m_pGettedBufferMark += resReadSize;
		}
		else if (m_UsedBufferSize > 0)
		{
			if ((m_pLastMoveMark - m_pGettedBufferMark) >= m_UsedBufferSize)
			{
				resReadSize = m_UsedBufferSize;
				pResult = m_pGettedBufferMark;
				m_pGettedBufferMark += m_UsedBufferSize;
			}
			else
			{
				resReadSize = static_cast<int>(m_pLastMoveMark - m_pGettedBufferMark);
				pResult = m_pGettedBufferMark;
				m_pGettedBufferMark += resReadSize;
			}
		}

		return pResult;
	}

	char* RingBuffer::ForwardMark(const int forwardLength)
	{
		SpinLock Lock(&m_CS);

		char* pPreCurMark = nullptr;
		if (m_UsedBufferSize + forwardLength > m_RingBufferSize)
		{
			return pPreCurMark;
		}

		if ((m_pEndMark - m_pCurMark) >= forwardLength)
		{
			pPreCurMark = m_pCurMark;
			m_pCurMark += forwardLength;
		}
		else
		{
			m_pLastMoveMark = m_pCurMark;
			m_pCurMark = m_pBeginMark + forwardLength;
			pPreCurMark = m_pBeginMark;
		}

		return pPreCurMark;
	}

	char* RingBuffer::ForwardMark(const int forwardLength, const int nextLength, const DWORD remainLength)
	{
		SpinLock Lock(&m_CS);

		if (m_UsedBufferSize + forwardLength + nextLength > m_RingBufferSize)
		{
			return nullptr;
		}

		if ((m_pEndMark - m_pCurMark) >= (nextLength + forwardLength))
		{
			m_pCurMark += forwardLength;
		}
		else
		{
			m_pLastMoveMark = m_pCurMark;

			CopyMemory(m_pBeginMark, m_pCurMark - (remainLength - forwardLength), remainLength);
			m_pCurMark = m_pBeginMark + remainLength;
		}

		return m_pCurMark;
	}

	void RingBuffer::ReleaseBuffer(const int releaseSize)
	{
		SpinLock Lock(&m_CS);

		m_UsedBufferSize = (releaseSize > m_UsedBufferSize) ? 0 : (m_UsedBufferSize - releaseSize);
	}

	void RingBuffer::SetUsedBufferSize(const int usedBufferSize)
	{
		SpinLock Lock(&m_CS);

		m_UsedBufferSize += usedBufferSize;
	}
}