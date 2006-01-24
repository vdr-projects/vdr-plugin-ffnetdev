/*
 * ffnetdevsetup.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id$
 */
 

#include "ffnetdevsetup.h"
#include "config.h"

cFFNetDevSetup::cFFNetDevSetup(void)
{

  m_iAutoSetPrimaryDVB         = config.iAutoSetPrimaryDVB;
  
  Add(new cMenuEditBoolItem(tr("auto set as primary device"),                 &m_iAutoSetPrimaryDVB, tr("no"), tr("yes")));
}

void cFFNetDevSetup::Store(void)
{
  SetupStore("AutoSetPrimaryDVB",         config.iAutoSetPrimaryDVB = m_iAutoSetPrimaryDVB);
}


