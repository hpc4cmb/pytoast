/*
Copyright (c) 2015-2017 by the parties listed in the AUTHORS file.
All rights reserved.  Use of this source code is governed by
a BSD-style license that can be found in the LICENSE file.
*/


#include "toast_util_internal.hpp"
#include <cassert>
#include <algorithm>

namespace toast
{
namespace util
{
namespace details
{

//============================================================================//

base_timer::mutex_map_t base_timer::w_mutex_map;

//============================================================================//

base_timer::base_timer(uint16_t prec, const string_t& fmt, std::ostream* os)
: m_valid_times(false),
  m_running(false),
  m_precision(prec),
  m_os(os),
  m_format_positions(pos_list_t()),
  m_format_string(fmt),
  m_output_format("")
{
    this->start();
    this->parse_format();
}

//============================================================================//

base_timer::~base_timer()
{
    if(!m_valid_times)
    {
        this->stop();
        if(m_os != &std::cout && *m_os)
            this->report();
    }
}

//============================================================================//

void base_timer::parse_format()
{
    size_type npos = std::string::npos;

    str_list_t fmts;
    fmts.push_back(clockstr_t("%w", WALL   ));
    fmts.push_back(clockstr_t("%u", USER   ));
    fmts.push_back(clockstr_t("%s", SYSTEM ));
    fmts.push_back(clockstr_t("%t", CPU    ));
    fmts.push_back(clockstr_t("%p", PERCENT));

    for(str_list_t::iterator itr = fmts.begin(); itr != fmts.end(); ++itr)
    {
        size_type pos = 0;
        // start at zero and look for all instances of string
        while((pos = m_format_string.find(itr->first, pos)) != npos)
        {
            // post-increment pos so we don't find same instance next
            // time around
            m_format_positions.push_back(clockpos_t(pos++, itr->second));
        }
    }
    std::sort(m_format_positions.begin(), m_format_positions.end(),
              [] (const clockpos_t& lhs, const clockpos_t& rhs)
              { return lhs.first < rhs.first; });
}

//============================================================================//

void base_timer::report(std::ostream& os, bool endline, bool avg) const
{
    // stop, if not already stopped
    if(!m_valid_times)
        const_cast<base_timer*>(this)->stop();

    // for average reporting
    double div = 1.0;
    if(avg && this->laps() > 0)
        div = 1.0 / static_cast<double>(this->laps());

    // use stringstream so precision and fixed don't directly affect
    // ostream
    std::stringstream ss;
    // set precision
    ss.precision(m_precision);
    // output fixed
    ss << std::fixed;
    size_type pos = 0;
    for(size_type i = 0; i < m_format_positions.size(); ++i)
    {
        // where to terminate the sub-string
        size_type ter = m_format_positions.at(i).first;
        assert(!(ter < pos));
        // length of substring
        size_type len = ter - pos;
        // create substring
        string_t substr = m_format_string.substr(pos, len);
        // add sub-string
        ss << substr;
        // print the appropriate timing mechanism
        switch (m_format_positions.at(i).second)
        {
            case WALL:
                // the real elapsed time
                ss << std::setw(3+m_precision)
                   << (real_elapsed() * div);
                break;
            case USER:
                // CPU time of non-system calls
                ss << std::setw(3+m_precision)
                   << (user_elapsed() * div);
                break;
            case SYSTEM:
                // thread specific CPU time, e.g. thread creation overhead
                ss << std::setw(3+m_precision)
                   << (system_elapsed() * div);
                break;
            case CPU:
                // total CPU time
                ss << std::setw(3+m_precision)
                   << ((user_elapsed() + system_elapsed()) * div);
                break;
            case PERCENT:
                // percent CPU utilization
                ss.precision(1);
                ss << ((user_elapsed()+system_elapsed())/real_elapsed())*100.0;
                break;
        }
        // skip over %{w,u,s,t,p} field
        pos = m_format_positions.at(i).first+2;
    }
    // write the end of the string
    size_type ter = m_format_string.length();
    size_type len = ter - pos;
    string_t substr = m_format_string.substr(pos, len);
    ss << substr;
    if(avg)
        ss << " (average of " << this->laps() << " laps)";
    if(endline)
        ss << std::endl;

    // ensure thread-safety
    auto_lock_t lock(w_mutex_map[&os]);
    // output to ostream
    os << ss.str();
}

} // namespace details

} // namespace util

} // namespace toast

//============================================================================//


