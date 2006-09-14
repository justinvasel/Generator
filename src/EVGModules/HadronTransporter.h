//____________________________________________________________________________
/*!

\class    genie::HadronTransporter

\brief    Intranuclear hadronic transport module. 
          It is being used to transfer all hadrons outside the nucleus without
          rescattering -if rescattering is switched off- or to call one of the 
          supported hadron transport MCs -if rescattering is switched on-
         
\author   Costas Andreopoulos <C.V.Andreopoulos@rl.ac.uk> CCLRC, Rutherford Lab

\created  September 14, 2006

\cpright  Copyright (c) 2003-2006, GENIE Neutrino MC Generator Collaboration
          All rights reserved.
          For the licensing terms see $GENIE/USER_LICENSE.
*/
//____________________________________________________________________________

#ifndef _HADRON_TRANSPORTER_H_
#define _HADRON_TRANSPORTER_H_

#include "EVGCore/EventRecordVisitorI.h"

namespace genie {

class HadronTransporter : public EventRecordVisitorI {

public :
  HadronTransporter();
  HadronTransporter(string config);
  ~HadronTransporter();

  //-- implement the EventRecordVisitorI interface
  void ProcessEventRecord(GHepRecord * event_rec) const;

  //-- override the Algorithm::Configure methods to load configuration
  //   data to private data members
  void Configure (const Registry & config);
  void Configure (string param_set);

private:

  void  LoadConfig                (void);
  void  TransportInTransparentNuc (GHepRecord * ev) const;
  void  GenerateVertex            (GHepRecord * ev) const;

  bool  fRescatON; // intranuclear rescattering turned on?
  const EventRecordVisitorI * fHadTranspModel; ///< hadron transport MC to use

};

}      // genie namespace

#endif // _INTRANUKE_H_
