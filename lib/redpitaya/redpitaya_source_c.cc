/* -*- c++ -*- */
/*
 * Copyright 2015 Pavel Demin
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

#include <fstream>
#include <string>
#include <sstream>
#include <stdexcept>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include <boost/assign.hpp>
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>

#include <gnuradio/io_signature.h>

#include "redpitaya_common.h"
#include "redpitaya_source_c.h"

#include "arg_helpers.h"

using namespace boost::assign;

redpitaya_source_c_sptr make_redpitaya_source_c(const std::string &args)
{
  return gnuradio::get_initial_sptr(new redpitaya_source_c(args));
}

redpitaya_source_c::redpitaya_source_c(const std::string &args) :
  gr::hier_block2("redpitaya_source_c",
                  gr::io_signature::make(0, 0, 0),
                  gr::io_signature::make(1, 1, sizeof(gr_complex)))
{
  std::string host = "192.168.1.100";
  std::stringstream message;
  unsigned short port = 1001;
  struct sockaddr_in addr;
  uint32_t command;

#ifdef _WIN32
  WSADATA wsaData;
  WSAStartup( MAKEWORD(2, 2), &wsaData );
#endif

  _freq = 6.0e5;
  _rate = 1.0e5;
  _corr = 0.0;

  dict_t dict = params_to_dict( args );

  if ( dict.count( "redpitaya" ) )
  {
    std::vector< std::string > tokens;
    boost::algorithm::split( tokens, dict["redpitaya"], boost::is_any_of( ":" ) );

    if ( tokens[0].length() && ( tokens.size() == 1 || tokens.size() == 2 ) )
      host = tokens[0];

    if ( tokens.size() == 2 )
      port = boost::lexical_cast< unsigned short >( tokens[1] );
  }

  if ( !host.length() )
    host = "192.168.1.100";

  if ( 0 == port )
    port = 1001;

  for ( size_t i = 0; i < 2; ++i )
  {
    if ( ( _sockets[i] = socket( AF_INET, SOCK_STREAM, 0 ) ) < 0 )
    {
      throw std::runtime_error( "Could not create TCP socket." );
    }

    memset( &addr, 0, sizeof(addr) );
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr( host.c_str() );
    addr.sin_port = htons( port );

    if ( ::connect( _sockets[i], (struct sockaddr *)&addr, sizeof(addr) ) < 0 )
    {
      message << "Could not connect to " << host << ":" << port << ".";
      throw std::runtime_error( message.str() );
    }

    command = i;
    redpitaya_send_command( _sockets[i], command );
  }

  _source = gr::blocks::file_descriptor_source::make( sizeof(gr_complex), _sockets[1] );

  connect( _source, 0, self(), 0 );
}

redpitaya_source_c::~redpitaya_source_c()
{
  close( _sockets[1] );
  close( _sockets[0] );

#ifdef _WIN32
  WSACleanup();
#endif
}

std::string redpitaya_source_c::name()
{
  return "Red Pitaya Source";
}

std::vector<std::string> redpitaya_source_c::get_devices( bool fake )
{
  std::vector<std::string> devices;

  if ( fake )
  {
    std::string args = "redpitaya=192.168.1.100:1001";
    args += ",label='Red Pitaya Transceiver Server'";
    devices.push_back( args );
  }

  return devices;
}

size_t redpitaya_source_c::get_num_channels( void )
{
  return 1;
}

osmosdr::meta_range_t redpitaya_source_c::get_sample_rates( void )
{
  osmosdr::meta_range_t range;

  range += osmosdr::range_t( 20000 );
  range += osmosdr::range_t( 50000 );
  range += osmosdr::range_t( 100000 );
  range += osmosdr::range_t( 250000 );
  range += osmosdr::range_t( 500000 );
  range += osmosdr::range_t( 1250000 );

  return range;
}

double redpitaya_source_c::set_sample_rate( double rate )
{
  uint32_t command = 0;

  if ( 20000 == rate ) command = 0;
  else if ( 50000 == rate ) command = 1;
  else if ( 100000 == rate ) command = 2;
  else if ( 250000 == rate ) command = 3;
  else if ( 500000 == rate ) command = 4;
  else if ( 1250000 == rate ) command = 5;
  else return get_sample_rate();

  command |= 1<<28;
  redpitaya_send_command( _sockets[0], command );

  _rate = rate;

  return get_sample_rate();
}

double redpitaya_source_c::get_sample_rate( void )
{
  return _rate;
}

osmosdr::freq_range_t redpitaya_source_c::get_freq_range( size_t chan )
{
  return osmosdr::freq_range_t( _rate / 2.0, 6.0e7 );
}

double redpitaya_source_c::set_center_freq( double freq, size_t chan )
{
  uint32_t command = 0;

  if ( freq < _rate / 2.0 || freq > 6.0e7 ) return get_center_freq( chan );

  command = (uint32_t)floor( freq * (1.0 + _corr * 1.0e-6 ) + 0.5 );

  redpitaya_send_command( _sockets[0], command );

  _freq = freq;

  return get_center_freq( chan );
}

double redpitaya_source_c::get_center_freq( size_t chan )
{
  return _freq;
}

double redpitaya_source_c::set_freq_corr( double ppm, size_t chan )
{
  _corr = ppm;

  return get_freq_corr( chan );
}

double redpitaya_source_c::get_freq_corr( size_t chan )
{
  return _corr;
}

std::vector<std::string> redpitaya_source_c::get_gain_names( size_t chan )
{
  return std::vector< std::string >();
}

osmosdr::gain_range_t redpitaya_source_c::get_gain_range( size_t chan )
{
  return osmosdr::gain_range_t();
}

osmosdr::gain_range_t redpitaya_source_c::get_gain_range( const std::string & name, size_t chan )
{
  return get_gain_range( chan );
}

double redpitaya_source_c::set_gain( double gain, size_t chan )
{
  return get_gain( chan );
}

double redpitaya_source_c::set_gain( double gain, const std::string & name, size_t chan )
{
  return set_gain( chan );
}

double redpitaya_source_c::get_gain( size_t chan )
{
  return 0;
}

double redpitaya_source_c::get_gain( const std::string & name, size_t chan )
{
  return get_gain( chan );
}

std::vector< std::string > redpitaya_source_c::get_antennas( size_t chan )
{
  return std::vector< std::string >();
}

std::string redpitaya_source_c::set_antenna( const std::string & antenna, size_t chan )
{
  return get_antenna( chan );
}

std::string redpitaya_source_c::get_antenna( size_t chan )
{
  return "RX";
}