#include "monitorcontainer.h"

#include "../include/afanasy.h"

#include "../libafanasy/blockdata.h"
#include "../libafanasy/job.h"
#include "../libafanasy/msgclasses/mcgeneral.h"
#include "../libafanasy/msgclasses/mctasksprogress.h"
#include "../libafanasy/msg.h"

#include "afcommon.h"
#include "monitoraf.h"
#include "useraf.h"

#define AFOUTPUT
#undef AFOUTPUT
#include "../include/macrooutput.h"

MonitorContainer::MonitorContainer():
   ClientContainer( "Monitors", AFTALK::MAXCOUNT),
   events( NULL),
   eventsCount( 0),
   jobEvents( NULL),
   jobEventsUids( NULL),
   jobEventsCount( 0)
{
   eventsCount = af::Msg::TMonitorCommonEvents_END - af::Msg::TMonitorCommonEvents_BEGIN - 1;
   events = new std::list<int32_t>[ eventsCount];
   jobEventsCount = af::Msg::TMonitorJobEvents_END - af::Msg::TMonitorJobEvents_BEGIN - 1;
   jobEvents     = new std::list<int32_t>[ jobEventsCount];
   jobEventsUids = new std::list<int32_t>[ jobEventsCount];
AFINFA("MonitorContainer::MonitorContainer: Events Count = %d, Job Events = %d\n", eventsCount, jobEventsCount);
}

MonitorContainer::~MonitorContainer()
{
AFINFO("MonitorContainer::~MonitorContainer:\n");
   if( events        != NULL ) delete [] events;
   if( jobEvents     != NULL ) delete [] jobEvents;
   if( jobEventsUids != NULL ) delete [] jobEventsUids;
}

af::Msg * MonitorContainer::addMonitor( MonitorAf *newMonitor)
{
   int id = addClient( newMonitor, true, this, af::Msg::TMonitorMonitorsDel);
   if( id != 0 )
   {
      AFCommon::QueueLog("Monitor registered: " + newMonitor->generateInfoString( false));
      addEvent( af::Msg::TMonitorMonitorsAdd, id);
   }
   return new af::Msg( af::Msg::TMonitorId, id);
}

bool MonitorContainer::setInterest( int type, af::MCGeneral & ids)
{
   int monitorId = ids.getId();
   if( monitorId == 0 )
   {
      AFCommon::QueueLogError("MonitorContainer::action: Zero monitor ID.");
      return false;
   }
   MonitorContainerIt mIt( this);
   MonitorAf * monitor = mIt.getMonitor( monitorId);
   if( monitor == NULL )
   {
      AFCommon::QueueLogError("MonitorContainer::action: No monitor with id = " + af::itos( monitorId));
      return false;
   }

   if( monitor->setInterest( type, ids) == false ) return false;

   addEvent( af::Msg::TMonitorMonitorsChanged, monitorId);
   return true;
}

void MonitorContainer::addEvent( int type, int id)
{
   if(( type <= af::Msg::TMonitorCommonEvents_BEGIN) || ( type >= af::Msg::TMonitorCommonEvents_END))
   {
      AFCommon::QueueLogError( std::string("MonitorContainer::addEvent: Invalid event type = ") + af::Msg::TNAMES[type]);
      return;
   }
//printf("MonitorContainer::addEvent: [%s] (%d<%d<%d)\n", af::Msg::TNAMES[type], af::Msg::TMonitorCommonEvents_BEGIN, type, af::Msg::TMonitorCommonEvents_END);
   int typeNum = type - af::Msg::TMonitorCommonEvents_BEGIN -1;
   addUniqueToList( events[typeNum], id);
}

void MonitorContainer::addJobEvent( int type, int id, int uid)
{
   if(( type <= af::Msg::TMonitorJobEvents_BEGIN) || ( type >= af::Msg::TMonitorJobEvents_END))
   {
      AFCommon::QueueLogError( std::string("MonitorContainer::addJobEvent: Invalid job event type = ") + af::Msg::TNAMES[type]);
      return;
   }
//printf("MonitorContainer::addJobEvent: [%s] (%d<%d<%d)\n", af::Msg::TNAMES[type], af::Msg::TMonitorCommonEvents_BEGIN, type, af::Msg::TMonitorCommonEvents_END);
   int typeNum = type - af::Msg::TMonitorJobEvents_BEGIN -1;
   if( addUniqueToList( jobEvents[typeNum], id)) jobEventsUids[typeNum].push_back( uid);
}

bool MonitorContainer::addUniqueToList( std::list<int32_t> & list, int value)
{
   std::list<int32_t>::iterator it = list.begin();
   std::list<int32_t>::iterator end_it = list.end();
   while( it != end_it) if( (*it++) == value) return false;
   list.push_back( value);
   return true;
}

void MonitorContainer::dispatch()
{
   //
   // Common Events:
   //
   for( int e = 0; e < eventsCount; e++)
   {
      if( events[e].size() < 1) continue;

      int eventType = af::Msg::TMonitorCommonEvents_BEGIN+1 + e;
//printf("MonitorContainer::dispatch: [%s]\n", af::Msg::TNAMES[eventType]);
      af::Msg * msg = NULL;
//      af::Msg * msg = new af::Msg( eventType, &mcIds );
//      bool founded = false;
      MonitorContainerIt monitorsIt( this);
      for( MonitorAf * monitor = monitorsIt.monitor(); monitor != NULL; monitorsIt.next(), monitor = monitorsIt.monitor())
      {
         if( monitor->hasEvent( eventType))
         {
            if( msg == NULL )
            {
               af::MCGeneral mcIds;
               collectIds( events[e], mcIds );
               msg = new af::Msg( eventType, &mcIds );
            }
            msg->addAddress( monitor);
         }
      }
      if( msg )
      {
          AFCommon::QueueMsgDispatch( msg);
//printf("MonitorContainer::dispatch: ");msg->stdOut();
      }
   }

   //
   // Job Events, which depend on interested users ids:
   //
   for( int e = 0; e < jobEventsCount; e++)
   {
      if( jobEvents[e].size() < 1) continue;
      int eventType = af::Msg::TMonitorJobEvents_BEGIN+1 + e;
      if( jobEvents[e].size() != jobEventsUids[e].size())
      {
         AFCommon::QueueLogError( std::string("MonitorContainer::dispatch: \
            jobEvents and jobEventsUids has different sizes fo [")
            + af::Msg::TNAMES[eventType] + "] ( " + af::itos(jobEvents[e].size()) + " != " + af::itos(jobEventsUids[e].size()) + " )");
         continue;
      }

      MonitorContainerIt monitorsIt( this);
      for( MonitorAf * monitor = monitorsIt.monitor(); monitor != NULL; monitorsIt.next(), monitor = monitorsIt.monitor())
      {
         af::MCGeneral mcIds;
         bool founded = false;

         std::list<int32_t>::const_iterator jIt = jobEvents[e].begin();
         std::list<int32_t>::const_iterator jIt_end = jobEvents[e].end();
         std::list<int32_t>::const_iterator uIt = jobEventsUids[e].begin();

         while( jIt != jIt_end)
         {
            if( monitor->hasJobEvent( eventType, *uIt))
            {
               if( !founded) founded = true;
               mcIds.addId( *jIt);
            }
            jIt++;
            uIt++;
         }
         if( ! founded ) continue;

         af::Msg * msg = new af::Msg( eventType, &mcIds );
         msg->setAddress( monitor);
         AFCommon::QueueMsgDispatch( msg);
      }
   }

   //
   // Tasks events:
   //
   std::list<af::MCTasksProgress*>::const_iterator tIt = tasks.begin();
   while( tIt != tasks.end())
   {
      af::Msg * msg = NULL;
      MonitorContainerIt monitorsIt( this);
      for( MonitorAf * monitor = monitorsIt.monitor(); monitor != NULL; monitorsIt.next(), monitor = monitorsIt.monitor())
      {
         if( monitor->hasJobId( (*tIt)->getJobId()))
         {
            if( msg == NULL)
            {
               msg = new af::Msg( af::Msg::TTasksRun, *tIt);
            }
            msg->addAddress( monitor);
         }
      }
      if( msg )
      {
          AFCommon::QueueMsgDispatch( msg);
      }
      tIt++;
   }

   //
   // Blocks changed:
   //
   {
   MonitorContainerIt monitorsIt( this);
   for( MonitorAf * monitor = monitorsIt.monitor(); monitor != NULL; monitorsIt.next(), monitor = monitorsIt.monitor())
   {
      af::MCAfNodes mcblocks;
      int type = 0;
      std::list<af::BlockData*>::iterator bIt = blocks.begin();
      std::list<int32_t>::iterator tIt = blocks_types.begin();
      for( ; bIt != blocks.end(); bIt++, tIt++)
      {
         if( monitor->hasJobId( (*bIt)->getJobId()))
         {
            mcblocks.addNode( (*bIt));
            if( type < *tIt) type = *tIt;
         }
      }
      if( mcblocks.getCount() < 1) continue;

      af::Msg * msg = new af::Msg( type, &mcblocks);
      msg->setAddress( monitor);
      AFCommon::QueueMsgDispatch( msg);
   }
   }

   //
   // Users jobs order:
   //
   {
   MonitorContainerIt monitorsIt( this);
   for( MonitorAf * monitor = monitorsIt.monitor(); monitor != NULL; monitorsIt.next(), monitor = monitorsIt.monitor())
   {
      std::list<UserAf*>::iterator uIt = users.begin();
      while( uIt != users.end())
      {
         if( monitor->hasJobUid((*uIt)->getId()))
         {
            af::MCGeneral ids;
            (*uIt)->generateJobsIds( ids);
            af::Msg * msg = new af::Msg( af::Msg::TUserJobsOrder, &ids);
            msg->setAddress( monitor);
            AFCommon::QueueMsgDispatch( msg);
         }
         uIt++;
      }
   }
   }

   //
   // Delete all events:
   //
   clearEvents();
}

bool MonitorContainer::collectIds( const std::list<int32_t> & list, af::MCGeneral & ids)
{
   std::list<int32_t>::const_iterator it = list.begin();
   std::list<int32_t>::const_iterator end_it = list.end();
   while( it != end_it) ids.addId( *(it++));
   if( ids.getCount()) return true;
   else return false;
}

void MonitorContainer::clearEvents()
{
   for( int e = 0; e < eventsCount; e++) events[e].clear();

   for( int e = 0; e < jobEventsCount; e++)
   {
      jobEvents[e].clear();
      jobEventsUids[e].clear();
   }

   std::list<af::MCTasksProgress*>::iterator tIt = tasks.begin();
   while( tIt != tasks.end())
   {
      delete *tIt;
      tIt++;
   }

   tasks.clear();

   blocks.clear();
   blocks_types.clear();

   users.clear();
}

void MonitorContainer::sendMessage( const af::MCGeneral & mcgeneral)
{
//printf("MonitorContainer::sendMessage:\n"); mcgeneral.stdOut( true);
   int idscount = mcgeneral.getCount();
   if( idscount < 1 ) return;

   af::Msg * msg = new af::Msg;
   bool founded = false;
   MonitorContainerIt mIt( this);
   for( int i = 0; i < idscount; i++)
   {
      MonitorAf * monitor = mIt.getMonitor( mcgeneral.getId(i));
      if( monitor == NULL) continue;
      msg->addAddress( monitor);
      if( !founded) founded = true;
   }
   if( founded)
   {
      msg->setString( mcgeneral.getUserName() + ": " + mcgeneral.getString() );
      AFCommon::QueueMsgDispatch( msg);
   }
   else delete msg;
}

void MonitorContainer::sendMessage( const std::string & str)
{
   af::Msg * msg = new af::Msg;
   msg->setString( str);
   MonitorContainerIt mIt( this);

   for( MonitorAf * monitor = mIt.monitor(); monitor != NULL; mIt.next(), monitor = mIt.monitor())
      msg->addAddress( monitor);
   if( msg->addressesCount() )
       AFCommon::QueueMsgDispatch( msg);
   else delete msg;
}

void MonitorContainer::addTask( int jobid, int block, int task, af::TaskProgress * tp)
{
   af::MCTasksProgress * t = NULL;

   std::list<af::MCTasksProgress*>::const_iterator tIt = tasks.begin();
   while( tIt != tasks.end())
   {
      if( (*tIt)->getJobId() == jobid)
      {
         t = (*tIt);
         break;
      }
      tIt++;
   }

   if( t == NULL )
   {
      t = new af::MCTasksProgress( jobid);
      tasks.push_back( t);
   }

   t->add( block, task, tp);
}

void MonitorContainer::addBlock( int type, af::BlockData * block)
{
   std::list<af::BlockData*>::const_iterator bIt = blocks.begin();
   std::list<int32_t>::iterator tIt = blocks_types.begin();
   for( ; bIt != blocks.end(); bIt++, tIt++)
   {
      if( block == *bIt)
      {
         if( type < *tIt ) *tIt = type;
         return;
      }
   }
   blocks.push_back( block);
   blocks_types.push_back( type);
}

void MonitorContainer::addUser( UserAf * user)
{
   std::list<UserAf*>::const_iterator uIt = users.begin();
   while( uIt != users.end()) if( *(uIt++) == user) return;
   users.push_back( user);
}

//##############################################################################
MonitorContainerIt::MonitorContainerIt( MonitorContainer* container, bool skipZombies):
   AfContainerIt( (AfContainer*)container, skipZombies)
{
}

MonitorContainerIt::~MonitorContainerIt()
{
}