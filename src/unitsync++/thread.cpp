#include "thread.h"
#include <algorithm>
#include <utils/logging.h>

namespace
{
	struct WorkItemCompare
	{
        bool operator()( const LSL::WorkItem* a, const LSL::WorkItem* b )
		{
			return a->GetPriority() < b->GetPriority();
		}
	};
}

namespace LSL {

bool WorkItem::Cancel()
{
    LslDebug( "cancelling WorkItem %p", this );
	if ( m_queue == NULL ) return false;
	return m_queue->Remove( this );
}

void WorkItemQueue::Push( WorkItem* item )
{
	if ( item == NULL ) return;
    boost::mutex::scoped_lock lock( m_lock );
	m_queue.push_back( item );
	std::push_heap( m_queue.begin(), m_queue.end(), WorkItemCompare() );
	item->m_queue = this;
}

WorkItem* WorkItemQueue::Pop()
{
    boost::mutex::scoped_lock lock( m_lock );
	if ( m_queue.empty() ) return NULL;
	WorkItem* item = m_queue.front();
	std::pop_heap( m_queue.begin(), m_queue.end(), WorkItemCompare() );
	m_queue.pop_back();
	item->m_queue = NULL;
	return item;
}

bool WorkItemQueue::Remove( WorkItem* item )
{
    boost::mutex::scoped_lock lock( m_lock );
	if ( m_queue.empty() ) return false;
	// WARNING: this destroys the heap...
	std::vector<WorkItem*>::iterator new_end =
    std::remove( m_queue.begin(), m_queue.end(), item );
	// did a WorkerThread process the item just before we got here?
	if ( new_end == m_queue.end() ) return false;
	m_queue.erase( new_end, m_queue.end() );
	// recreate the heap...
	std::make_heap( m_queue.begin(), m_queue.end(), WorkItemCompare() );
	item->m_queue = NULL;
	return true;
}


void WorkerThread::DoWork( WorkItem* item, int priority, bool toBeDeleted )
{
    LslDebug( "scheduling WorkItem %p, prio = %d", item, priority );
	item->m_priority = priority;
	item->m_toBeDeleted = toBeDeleted;
	m_workItems.Push( item );
    m_cond.notify_one();
}

void WorkerThread::Wait()
{
    if ( m_thread )
        m_thread->join();
}

void WorkerThread::operator()()
{
    while ( true ) {
        WorkItem* item = NULL;
        boost::unique_lock<boost::mutex> lock(m_mutex);
        while ( item = m_workItems.Pop() ) {
            try {
                LslDebug( "running WorkItem %p, prio = %d", item, item->m_priority );
                item->Run();
            }
            catch ( std::exception& e ) {
                // better eat all exceptions thrown by WorkItem::Run(),
                // don't want to let the thread die on a single faulty WorkItem.
                LslDebug( "WorkerThread caught exception thrown by WorkItem::Run -- %s", e.what() );
            } catch ( ... ) {
                LslDebug( "WorkerThread caught exception thrown by WorkItem::Run");
            }
            CleanupWorkItem( item );
        }
        // cleanup leftover WorkItems
        while ( ( item = m_workItems.Pop() ) != NULL ) {
            CleanupWorkItem( item );
        }
        //wait for the next Push
        m_cond.wait( lock );
    }
}

void WorkerThread::CleanupWorkItem( WorkItem* item )
{
	if ( item->m_toBeDeleted ) {
		try {
			delete item;
		}
        catch ( std::exception& e ) {
			// idem, eat exceptions from destructor
            LslDebug( "WorkerThread caught exception thrown by WorkItem::~WorkItem -- %s", e.what() );
		}
	}
}

} // namespace LSL
