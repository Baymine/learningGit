#include <time.h>
#include <sys/time.h>
#include <sstream>
#include <sys/syscall.h>
#include <unistd.h>
#include <iostream>
#include <stdio.h>

#include "log.h"
#include "../coroutine/coroutine.h"


namespace tinyrpc {

class Coroutine;

static thread_local pid_t t_thread_id = 0;
static pid_t g_pid = 0;

LogLevel g_log_level = DEBUG;

pid_t gettid() {
  if (t_thread_id == 0) {
    t_thread_id = syscall(SYS_gettid);
  }
  return t_thread_id;
}

void setLogLevel(LogLevel level) {
  g_log_level = level;
}


LogEvent::LogEvent(LogLevel level, const char* file_name, int line, const char* func_name)
  : m_level(level),
    m_file_name(file_name),
    m_line(line),
    m_func_name(func_name) {


}

LogEvent::~LogEvent() {

}

void levelToString(LogLevel level, std::string& re) {
  switch(level) {
    case DEBUG:
      re = "DEBUG";
      return;
    
    case INFO:
      re = "INFO";
      return;

    case WARN:
      re = "WARN";
      return;

    case ERROR:
      re = "ERROR";
      return;

    default:
      re = "";
      return;
  }
}


std::stringstream& LogEvent::getStringStream() {

  // time_t now_time = m_timestamp;
  
  gettimeofday(&m_timeval, nullptr);

  struct tm time; 
  localtime_r(&(m_timeval.tv_sec), &time);

  const char* format = "%Y-%m-%d %H:%M:%S";
  char buf[128];
  strftime(buf, sizeof(buf), format, &time);

  m_ss << "[" << buf << "." << m_timeval.tv_usec << "]\t"; 

  std::string s_level;
  levelToString(m_level, s_level);
  m_ss << "[" << s_level << "]\t";

  if (g_pid == 0) {
    g_pid = getpid();
  }
  m_pid = g_pid;  

  if (t_thread_id == 0) {
    t_thread_id = gettid();
  }
  m_tid = t_thread_id;

  m_cor_id = Coroutine::GetCurrentCoroutine()->getCorId();
  
  m_ss << "[" << m_pid << "]\t" 
		<< "[" << m_tid << "]\t"
		<< "[" << m_cor_id << "]\t"
    << "[" << m_file_name << ":" << m_line << "]\t";
    // << "[" << m_func_name << "]\t";
  
  return m_ss;
}

void LogEvent::log() {
	m_ss << "\n";
  if (m_level >= g_log_level) {

    // Mutex::Lock lock(m_mutex);
    // std::cout << m_ss.str();
    printf(m_ss.str().c_str());
  }
}


LogTmp::LogTmp(LogEvent::ptr event) : m_event(event) {

}

std::stringstream& LogTmp::getStringStream() {
  return m_event->getStringStream();
}

LogTmp::~LogTmp() {
  m_event->log(); 
}

}