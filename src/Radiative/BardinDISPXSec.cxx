//____________________________________________________________________________
/*
 Copyright (c) 2003-2008, GENIE Neutrino MC Generator Collaboration
 For the full text of the license visit http://copyright.genie-mc.org
 or see $GENIE/LICENSE

 Author: Costas Andreopoulos <C.V.Andreopoulos@rl.ac.uk>
         STFC, Rutherford Appleton Laboratory - July 06, 2004

 For the class documentation see the corresponding header file.

 Important revisions after version 2.0.0 :

*/
//____________________________________________________________________________

#include <vector>

#include <TMath.h>

#include "Algorithm/AlgConfigPool.h"
#include "Base/XSecIntegratorI.h"
#include "Conventions/Constants.h"
#include "Conventions/RefFrame.h"
#include "Conventions/KineVar.h"
#include "Conventions/Units.h"
#include "Messenger/Messenger.h"
#include "PDF/PDFModelI.h"
#include "PDG/PDGCodes.h"
#include "PDG/PDGUtils.h"
#include "PDG/PDGLibrary.h"
#include "Radiative/BardinDISPXSec.h"
#include "Utils/MathUtils.h"
#include "Utils/KineUtils.h"
#include "Utils/Range1.h"

using std::vector;
using namespace genie;
using namespace genie::constants;

//____________________________________________________________________________
BardinDISPXSec::BardinDISPXSec() :
XSecAlgorithmI("genie::BardinDISPXSec")
{

}
//____________________________________________________________________________
BardinDISPXSec::BardinDISPXSec(string config) :
XSecAlgorithmI("genie::BardinDISPXSec", config)
{

}
//____________________________________________________________________________
BardinDISPXSec::~BardinDISPXSec()
{

}
//____________________________________________________________________________
double BardinDISPXSec::XSec(
                  const Interaction * interaction, KinePhaseSpace_t kps) const
{
  if(! this -> ValidProcess    (interaction) ) return 0.;
  if(! this -> ValidKinematics (interaction) ) return 0.;

  //----- get kinematical & init-state parameters
  const Kinematics &   kinematics = interaction -> Kine();
  const InitialState & init_state = interaction -> InitState();
  const Target & target = init_state.Tgt();

  double E  = init_state.ProbeE(kRfHitNucRest);
  double x  = kinematics.x();
  double y  = kinematics.y();
  double Q2 = S(interaction) * x * y;

  //----- Create PDF objects & attach PDFModels
  PDF pdf_x, pdf_xi;

  pdf_x.SetModel  (fPDFModel);   // <-- attach algorithm
  pdf_xi.SetModel (fPDFModel);   // <-- attach algorithm

  //----- Get init quark PDF at (x,Q2)
  pdf_x.Calculate(x, Q2);
  LOG("Bardin", pDEBUG) << pdf_x;
  int init_pdgc = target.HitQrkPdg();
  double f_x  = PDFFunc( pdf_x,  init_pdgc )  / x;

  //----- Compute the differential cross section terms (1-3)
  double term1 =  x * f_x * (1 + (kAem/kPi)*DeltaCCi(interaction));

  // internal loop : integration over xi for the given (x,y,E) xi e (x,1)
  // Define the range for variable xi
  //   Note: Integration over xi is performed for xi e [x, 1].
  //   During the numerical integration, When we compute values of the
  //   cross section terms at various xi, then :
  //   - xi = x can not be included because of a singularity { 1/(xi-x) term }
  //   - xi = 1 can not be included because PDF calculators complain
  const int    nxi   = 201;
  const double xi_min = x + 0.001;
  const double xi_max = 0.999;
  const double dxi   = (xi_max-xi_min)/(nxi-1);

  // This code here does not follow the standard GENIE numerical integration
  // techniques and offers no quarantee for the numerical convergence
  // Need to bring in sync with the rest...
  vector<double> dterm2_dxi(nxi);
  vector<double> dterm3_dxi(nxi);

  for(int ix = 0; ix < nxi; ix++) {
     double xi = xi_min + ix * dxi;

     //----- Get init quark PDF at (xi,Q2)
     pdf_xi.Calculate(xi, Q2);
     double f_xi = PDFFunc( pdf_xi, init_pdgc )  / xi;

     dterm2_dxi[ix] = f_xi * PhiCCi(xi, interaction);
     dterm3_dxi[ix] = (xi*f_xi*Ii(xi,interaction)-x*f_x*Ii(x, interaction))/(xi-x);
  }

  double term2 = (dterm2_dxi[0] + dterm2_dxi[nxi-1])/2;
  double term3 = (dterm3_dxi[0] + dterm3_dxi[nxi-1])/2;
  for(unsigned int i = 1; i< nxi-1; i++)  {
    term2 += (dterm2_dxi[i] * (i%2 + 1));
    term3 += (dterm3_dxi[i] * (i%2 + 1));
  }
  term2 *= (2.*dxi/3.);
  term3 *= (2.*dxi/3.);

  LOG("Bardin", pDEBUG) << "term1 = " << term1;
  LOG("Bardin", pDEBUG) << "term2 = " << term2;
  LOG("Bardin", pDEBUG) << "term3 = " << term3;

  //----- Compute the differential cross section d^2xsec/dxdy
  double Gfac = TMath::Power(kGF,2) * S(interaction) * fVud2 / kPi;
  double xsec = Gfac * ( term1 + (kAem/kPi) * (term2 + term3) );

  LOG("Bardin", pINFO)
     << "d2xsec/dxdy [DIS,FreeN](E = " << E << ", x = " << x
                                   << ", y = " << y << ") = " << xsec;
  assert(xsec >= 0);
 
  //----- The algorithm computes d^2xsec/dxdy
  //      Check whether variable tranformation is needed
  if(kps!=kPSxyfE) {
    double J = utils::kinematics::Jacobian(interaction,kPSxyfE,kps);
    xsec *= J;
  }

  //----- If requested return the free nucleon xsec even for input nuclear tgt
  if( interaction->TestBit(kIAssumeFreeNucleon) ) return xsec;

  //----- Compute nuclear cross section (simple scaling here)
  int nucpdgc = target.HitNucPdg();
  int NNucl = (pdg::IsProton(nucpdgc)) ? target.Z() : target.N();
  xsec *= NNucl;

  return xsec;
}
//____________________________________________________________________________
double BardinDISPXSec::PhiCCi(double xi, const Interaction * interaction) const
{
  const InitialState & init_state = interaction -> InitState();
  const Kinematics & kine = interaction->Kine();
  const Target & target = init_state.Tgt();

  double MN   = target.HitNucMass();
  double x    = kine.x();
  double y    = kine.y();
  int    pdg  = target.HitQrkPdg();
  double mqi  = xi*MN;
  double mqf2 = TMath::Power(fMqf,2);
  double mqi2 = TMath::Power(mqi,2);
  double ml   = interaction->FSPrimLepton()->Mass();
  double Q2   = S(interaction) * x * y;
  double t    = tau(xi, interaction);
  double st   = St(xi, interaction);
  double u    = U(xi, interaction);
  double su   = Su(xi, interaction);
  double f    = PDGLibrary::Instance()->Find(pdg)->Charge();
  double ml2  = ml * ml;
  double st2  = st * st;
  double u2   = u  * u;
  double f2   = f  * f;
  double xi2  = xi * xi;
  double x2   = x  * x;
  double y2   = y  * y;

  double term1 = pow(1+f, 2) * (0.25*Q2 /t) * (1 - mqf2/t);
  double term2 = 0.25 + 0.75*y - 0.5*y*(1 + xi/(xi-y*(xi-x)) ) * log(st2/(t*ml2));
  double term3 = f * ( 2 - (y*xi/(xi-y*(xi-x))) * log(st2/(t*ml2)) );
  double term4 = f * ( y*(y-2-y*x/xi) * log(st*u/(t*su)) + (0.5+y)*x/xi );
  double term5 = f2 * ( 2 - (1 + 0.5*x/xi + 0.5*x2/xi2) * log(u2/(t*mqi2)) );
  double term6 = f2 * ( 2 - 0.5*y + 0.25*y2 ) * x/xi;
  double term7 = f2 * (1.5 - 0.5*y - 0.25*y2) * x2/xi2;

  double phi   = term1 + term2 + term3 + term4 + term5 + term6 + term7;

  LOG("Bardin", pDEBUG) << "PhiCC = " << phi;

  return phi;
}
//__________________________________________________________________________
double BardinDISPXSec::DeltaCCi(const Interaction * interaction) const
{
  const InitialState & init_state = interaction -> InitState();
  const Kinematics & kine = interaction->Kine();
  const Target & target = init_state.Tgt();

  double MN   = init_state.Tgt().HitNucMass();
  double x    = kine.x();
  double y    = kine.y();
  int    pdg  = target.HitQrkPdg();
  double mqi  = x*MN;
  double mqf2 = TMath::Power(fMqf,2);
  double mqi2 = TMath::Power(mqi,2);
  double ml   = interaction->FSPrimLepton()->Mass();
  double ml2  = ml*ml;

  double s       = S(interaction);
  double sq      = Sq(interaction);
  double Iixx    = Ii(x, interaction);
  double sq_mqf2 = sq / mqf2;
  double sq_ml2  = sq / ml2;
  double sq_mZ2  = sq / kMz2;
  double Q2      = S(interaction) * x * y;

  double f       = PDGLibrary::Instance()->Find(pdg)->Charge();
  double f2      = f*f;

  LOG("Bardin", pDEBUG) << "ml      = " << ml;
  LOG("Bardin", pDEBUG) << "s       = " << s;
  LOG("Bardin", pDEBUG) << "sq      = " << sq;
  LOG("Bardin", pDEBUG) << "Iixx    = " << Iixx;
  LOG("Bardin", pDEBUG) << "sq_Mgf2 = " << sq_mqf2;
  LOG("Bardin", pDEBUG) << "sq_Ml2  = " << sq_ml2;
  LOG("Bardin", pDEBUG) << "sq_MZ2  = " << sq_mZ2;
  LOG("Bardin", pDEBUG) << "Q2      = " << Q2;
  LOG("Bardin", pDEBUG) << "f       = " << f;

  double delta =  Iixx * log(s * y * (1-x) / mqf2) + 0.75 +
                   (1.75 - log(sq_ml2)) * log(sq_mqf2) -
                   0.5 * pow( log(sq_mqf2), 2 ) +
                   pow(kPi, 2) / 2. -
                   1.5 * log(sq_mZ2) +
                   0.75 * log(sq_ml2) +
                   f * (1.75 + (1.5 + 2*log(1-y)) * log(Q2/mqf2) -
                   pow( log(Q2/mqf2), 2) +
                   pow(kPi, 2) / 2. -
                   1.5 * log(sq_mZ2 * (1-y)) -
                   log(y) * log(1-y) ) +
                   f2 * ( -1 + (1.75 - log(Q2/mqi2))*log(Q2/mqf2) -
                   0.5 * pow( log(Q2/mqf2), 2) + 0.75*log(Q2/mqi2));

  LOG("Bardin", pDEBUG) << "delta = " << delta;

  return delta;
}
//__________________________________________________________________________
double BardinDISPXSec::Ii(double xi, const Interaction * interaction) const
{
  const InitialState & init_state = interaction -> InitState();
  const Kinematics & kine = interaction->Kine();
  const Target & target = init_state.Tgt();

  double MN   = target.HitNucMass();
  double x    = kine.x();
  double y    = kine.y();
  int    pdg  = target.HitQrkPdg();
  double ml   = interaction->FSPrimLepton()->Mass();
  double mqi  = xi*MN;
  double st   = St(xi, interaction);
  double u    = U(xi, interaction);
  double su   = Su(xi, interaction);
  double t    = tau(xi, interaction);
  double f    = PDGLibrary::Instance()->Find(pdg)->Charge();
  double ml2  = ml  * ml;
  double mqi2 = mqi * mqi;
  double st2  = st  * st;
  double u2   = u   * u;
  double f2   = f   * f;

  double term1 = ( xi / (xi - y*(xi-x)) ) * TMath::Log(st2/(t*ml2)) - 2;
  double term2 = f * (2 * TMath::Log(st*u/(t*su)) - 2 +
                        ( y*(xi-x)/(xi-y*(xi-x)) ) * TMath::Log(st2/(t*ml2)) );
  double term3 = f2 * ( TMath::Log(u2/(t*mqi2)) - 2 );

  double I = term1 + term2 + term3;

  return I;
}
//__________________________________________________________________________
double BardinDISPXSec::S(const Interaction * interaction) const
{
  const InitialState & init_state = interaction->InitState();

  double E = init_state.ProbeE(kRfHitNucRest);
  double M = init_state.Tgt().HitNucMass();
  double S = 2*M*E;
  return S;
}
//__________________________________________________________________________
double BardinDISPXSec::U(double xi, const Interaction * interaction) const
{
  double y = interaction->Kine().y();
  return S(interaction) * y * xi;
}
//__________________________________________________________________________
double BardinDISPXSec::tau(double xi, const Interaction * interaction) const
{
  double x    = interaction->Kine().x();
  double y    = interaction->Kine().y();
  double mqf2 = TMath::Power(fMqf,2);

  return S(interaction) * y * (xi-x) + mqf2;
}
//__________________________________________________________________________
double BardinDISPXSec::St(double xi, const Interaction * interaction) const
{
  double x = interaction->Kine().x();
  double y = interaction->Kine().y();

  return S(interaction) * (xi - y*(xi-x));
}
//__________________________________________________________________________
double BardinDISPXSec::Su(double xi, const Interaction * interaction) const
{
  double y = interaction->Kine().y();
  return S(interaction) * xi * (1-y);
}
//__________________________________________________________________________
double BardinDISPXSec::Sq(const Interaction * interaction) const
{
  double x = interaction->Kine().x();
  return S(interaction) * x;
}
//__________________________________________________________________________
double BardinDISPXSec::PDFFunc(const PDF & pdf, int pdgc) const
{
  if     ( pdg::IsUQuark(pdgc) ) return (pdf.UpValence()   + pdf.UpSea()  );
  else if( pdg::IsDQuark(pdgc) ) return (pdf.DownValence() + pdf.DownSea());
  else {
     LOG("Bardin", pERROR)
         << "Scattering off quarks with pdg = " << pdgc << "is not handled";
  }
  return 0;
}
//__________________________________________________________________________
double BardinDISPXSec::Integral(const Interaction * interaction) const
{
  double xsec = fXSecIntegrator->Integrate(this,interaction);
  return xsec;
}
//____________________________________________________________________________
bool BardinDISPXSec::ValidProcess(const Interaction * interaction) const
{
  if(interaction->TestBit(kISkipProcessChk)) return true;

  const InitialState & init_state = interaction -> InitState();
  const ProcessInfo &  proc_info  = interaction -> ProcInfo();

  const Target & target = init_state.Tgt();
  if(!target.HitQrkIsSet()) return false;

  int nu  = init_state.ProbePdg();
  int qrk = target.HitQrkPdg();

  bool nqok = ( pdg::IsNeutrino(nu)     && pdg::IsDQuark(qrk) ) ||
              ( pdg::IsAntiNeutrino(nu) && pdg::IsUQuark(qrk) );
  if(!nqok) return false;

  bool prcok = proc_info.IsDeepInelastic() && proc_info.IsWeakCC();
  if(!prcok) return false;

  return true;
}
//__________________________________________________________________________
void BardinDISPXSec::Configure(const Registry & config)
{
  Algorithm::Configure(config);
  this->LoadConfig();
}
//__________________________________________________________________________
void BardinDISPXSec::Configure(string param_set)
{
  Algorithm::Configure(param_set);
  this->LoadConfig();
}
//__________________________________________________________________________
void BardinDISPXSec::LoadConfig(void)
{
  AlgConfigPool * confp = AlgConfigPool::Instance();
  const Registry * gc = confp->GlobalParameterList();

  fVud  = fConfig->GetDoubleDef("CKM-Vud", gc->GetDouble("CKM-Vud"));
  fVud2 = TMath::Power(fVud,2);

  fMqf  = fConfig->GetDoubleDef("Final-Quark-Mass", 0.1);

  //-- load the PDF model
  fPDFModel = dynamic_cast<const PDFModelI *> (this->SubAlg("PDF-Set"));
  assert(fPDFModel);

  //-- load the differential cross section integrator
  fXSecIntegrator =
      dynamic_cast<const XSecIntegratorI *> (this->SubAlg("XSec-Integrator"));
  assert(fXSecIntegrator);
}
//__________________________________________________________________________