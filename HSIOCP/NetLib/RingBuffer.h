#pragma once

namespace NetLib
{
	class RingBuffer
	{
	public:
		RingBuffer() {}
		~RingBuffer() { SAFE_DELETE_ARR(m_pRingBuffer); }

	public:
		bool Create(const int ringBufferSize);
		void Init(void);

		char* GetBuffer(const int reqReadSize, OUT int& resReadSize);
		char* ForwardMark(const int forwardLength);
		char* ForwardMark(const int forwardLength, const int nextLength, const DWORD remainLength);

		void ReleaseBuffer(const int releaseSize);
		void SetUsedBufferSize(const int usedBufferSize);

		int GetBufferSize(void) { return m_RingBufferSize; }
		char* GetBeginMark(void) { return m_pBeginMark; }
		char* GetCurMark(void) { return m_pCurMark; }

	private:
		CustomSpinLockCriticalSection m_CS;

		int m_RingBufferSize = INVALID_VALUE;
		int m_UsedBufferSize = INVALID_VALUE;
		char* m_pRingBuffer = nullptr;
		char* m_pBeginMark = nullptr;
		char* m_pEndMark = nullptr;
		char* m_pCurMark = nullptr;
		char* m_pGettedBufferMark = nullptr;
		char* m_pLastMoveMark = nullptr;
	};
}