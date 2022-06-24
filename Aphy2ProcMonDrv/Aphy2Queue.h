#pragma once
#include <ntddk.h>
template <class T2>
struct APHY2_QUEUE_NODE
{
	APHY2_QUEUE_NODE<T2>* Next;
	T2 Data;
};
template <class T1>
class APHY2_QUEUE
{
private:
	APHY2_QUEUE_NODE<T1>* head;
	APHY2_QUEUE_NODE<T1>* tail;
	unsigned long long size;
	PAGED_LOOKASIDE_LIST LookasideList;
	BOOLEAN bErr;
public:
	APHY2_QUEUE()
	{
		bErr = false;
		size = 0;
		head = tail = NULL;
		ExInitializePagedLookasideList(&LookasideList, NULL, NULL, 0, sizeof(APHY2_QUEUE_NODE<T1>), 0, 0);
	}
	void Aphy2QueuePush(const T1 data)
	{
		bErr = 0;
		APHY2_QUEUE_NODE<T1>* node = (APHY2_QUEUE_NODE<T1>*)ExAllocateFromPagedLookasideList(&LookasideList);
		if (!node)
		{
			bErr = 1;
			return;
		}
		node->Data = data;
		if (tail)tail->Next = node;
		tail = node;
		if (head == NULL)head = node;
		size++;
	}
	void Aphy2QueuePop(T1* data = NULL)
	{
		bErr = 0;
		if (size == 0)
		{
			return;
		}
		if (data)*data = head->Data;
		APHY2_QUEUE_NODE<T1>* node = head->Next;
		ExFreeToPagedLookasideList(&LookasideList, head);
		head = node;
		size--;
	}
	unsigned long long Aphy2QueueSize()
	{
		bErr = 0;
		return size;
	}
	BOOLEAN Aphy2QueueError()
	{
		return bErr;
	}
};