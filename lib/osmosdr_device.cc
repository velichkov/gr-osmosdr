/* -*- c++ -*- */
/*
 * Copyright 2012 Dimitri Stolnikov <horiz0n@gmx.net>
 *
 * GNU Radio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * GNU Radio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Radio; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#include <osmosdr_device.h>
#include <stdexcept>
#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <boost/thread/mutex.hpp>
#include <algorithm>
#include <sstream>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
/*
#define ENABLE_OSMOSDR
#define ENABLE_FCD
#define ENABLE_RTL
#define ENABLE_UHD
*/
#ifdef ENABLE_OSMOSDR
#include <osmosdr_src_c.h>
#endif

#ifdef ENABLE_FCD
#include <fcd_source.h>
#endif

#ifdef ENABLE_RTL
#include <rtl_source_c.h>
#endif

#ifdef ENABLE_UHD
#include <uhd_source_c.h>
#endif

#include "osmosdr_arg_helpers.h"

using namespace osmosdr;

static const std::string args_delim = " ";
static const std::string pairs_delim = ",";
static const std::string pair_delim = "=";

static boost::mutex _device_mutex;

device_t::device_t(const std::string &args)
{
  dict_t dict = params_to_dict(args);

  BOOST_FOREACH( dict_t::value_type &entry, dict )
    (*this)[entry.first] = entry.second;
}

std::string device_t::to_pp_string(void) const
{
  if (this->size() == 0) return "Empty Device Address";

  std::stringstream ss;
  ss << "Device Address:" << std::endl;
  BOOST_FOREACH(const device_t::value_type &entry, *this) {
    ss << boost::format("    %s: %s") % entry.first % entry.second << std::endl;
  }
  return ss.str();
}

std::string device_t::to_string(void) const
{
  std::stringstream ss;
  size_t count = 0;
  BOOST_FOREACH(const device_t::value_type &entry, *this) {
    std::string value = entry.second;
    if (value.find(" ") != std::string::npos)
      value = "'" + value + "'";
    ss << ((count++) ? pairs_delim : "") + entry.first + pair_delim + value;
  }
  return ss.str();
}

devices_t device::find(const device_t &hint)
{
  boost::mutex::scoped_lock lock(_device_mutex);

  devices_t devices;

#ifdef ENABLE_OSMOSDR
  BOOST_FOREACH( std::string dev, osmosdr_src_c::get_devices() )
    devices.push_back( device_t(dev) );
#endif
#ifdef ENABLE_FCD
  BOOST_FOREACH( std::string dev, fcd_source::get_devices() )
    devices.push_back( device_t(dev) );
#endif
#ifdef ENABLE_RTL
  BOOST_FOREACH( std::string dev, rtl_source_c::get_devices() )
    devices.push_back( device_t(dev) );
#endif
#ifdef ENABLE_UHD
  BOOST_FOREACH( std::string dev, uhd_source_c::get_devices() )
    devices.push_back( device_t(dev) );
#endif

  return devices;
}