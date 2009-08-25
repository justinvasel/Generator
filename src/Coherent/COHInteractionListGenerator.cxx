//____________________________________________________________________________
/*
 Copyright (c) 2003-2009, GENIE Neutrino MC Generator Collaboration
 For the full text of the license visit http://copyright.genie-mc.org
 or see $GENIE/LICENSE

 Author: Costas Andreopoulos <costas.andreopoulos \at stfc.ac.uk>
         STFC, Rutherford Appleton Laboratory - December 19, 2005

 For the class documentation see the corresponding header file.

 Important revisions after version 2.0.0 :
 @ Feb 09, 2009 - CA
   Moved into the new Coherent package from its previous location  (EVGModules 
   package)

*/
//____________________________________________________________________________

#include "Coherent/COHInteractionListGenerator.h"
#include "EVGCore/InteractionList.h"
#include "Interaction/Interaction.h"
#include "Messenger/Messenger.h"
#include "PDG/PDGCodes.h"
#include "PDG/PDGUtils.h"

using namespace genie;

//___________________________________________________________________________
COHInteractionListGenerator::COHInteractionListGenerator() :
InteractionListGeneratorI("genie::COHInteractionListGenerator")
{

}
//___________________________________________________________________________
COHInteractionListGenerator::COHInteractionListGenerator(string config) :
InteractionListGeneratorI("genie::COHInteractionListGenerator", config)
{

}
//___________________________________________________________________________
COHInteractionListGenerator::~COHInteractionListGenerator()
{

}
//___________________________________________________________________________
InteractionList * COHInteractionListGenerator::CreateInteractionList(
                                      const InitialState & init_state) const
{
  LOG("IntLst", pINFO)
      << "InitialState = " << init_state.AsString();

  InteractionType_t inttype;
  if      (fIsCC) inttype = kIntWeakCC;
  else if (fIsNC) inttype = kIntWeakNC;
  else {
     LOG("IntLst", pWARN)
       << "Unknown InteractionType! Returning NULL InteractionList "
                         << "for init-state: " << init_state.AsString();
     return 0;
  }

  int nupdg = init_state.ProbePdg();
  if( !pdg::IsNeutrino(nupdg) && !pdg::IsAntiNeutrino(nupdg) ) {
     LOG("IntLst", pWARN)
       << "Can not handle probe! Returning NULL InteractionList "
                         << "for init-state: " << init_state.AsString();
     return 0;
  }
  const Target & target = init_state.Tgt();
  if(!target.IsNucleus()) {
     LOG("IntLst", pWARN)
       << "Not a nuclear target! Returning NULL InteractionList "
                         << "for init-state: " << init_state.AsString();
     return 0;
  }

  InteractionList * intlist = new InteractionList;

  ProcessInfo proc_info(kScCoherent, inttype);
  Interaction * interaction = new Interaction(init_state, proc_info);

  if(fIsCC) {
    if(pdg::IsNeutrino(nupdg)) {
        // v A -> l- A pi+
        interaction->ExclTagPtr()->SetNPions(1,0,0);  
    } else 	{
        // vbar A -> l+ A pi-
        interaction->ExclTagPtr()->SetNPions(0,0,1); 
    }
  }
  else {
   // v A -> v A pi0
   interaction->ExclTagPtr()->SetNPions(0,1,0);
  } 

  intlist->push_back(interaction);

  return intlist;
}
//___________________________________________________________________________
void COHInteractionListGenerator::Configure(const Registry & config)
{
  Algorithm::Configure(config);
  this->LoadConfigData();
}
//____________________________________________________________________________
void COHInteractionListGenerator::Configure(string config)
{
  Algorithm::Configure(config);
  this->LoadConfigData();
}
//____________________________________________________________________________
void COHInteractionListGenerator::LoadConfigData(void)
{
  fIsCC = fConfig->GetBoolDef("is-CC", false);
  fIsNC = fConfig->GetBoolDef("is-NC", false);
}
//____________________________________________________________________________
