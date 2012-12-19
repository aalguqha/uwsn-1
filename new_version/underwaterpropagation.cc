
#include <stdio.h>
#include <topography.h>
#include "underwaterpropagation.h"
#include  "underwaterphy.h"
#include "math.h"
#include <algorithm>

class PacketStamp;


static class UnderwaterPropagationClass: public TclClass {
public:
  UnderwaterPropagationClass() : TclClass("Propagation/UnderwaterPropagation") {}
        TclObject* create(int, const char*const*) {
                return (new UnderwaterPropagation);
        }
} class_underwaterpropagation;




int
UnderwaterPropagation::command(int argc, const char*const* argv)
{
  TclObject *obj; 
 
  if(argc == 3) 
    {
      if( (obj = TclObject::lookup(argv[2])) == 0) 
	{
	  fprintf(stderr, "Propagation: %s lookup of %s failed\n", argv[1],
		  argv[2]);
	  return TCL_ERROR;
	}

      if (strcasecmp(argv[1], "topography") == 0) 
	{
	  topo = (Topography*) obj;
	  return TCL_OK;
	}
    }
  return TclObject::command(argc,argv);
}
 
double
UnderwaterPropagation::Pr(PacketStamp * t, PacketStamp * r, UnderwaterPhy * ifp)
{
	double F = ifp->getFrequency();	     // frequency range
	double K = ifp->getEnergyspread();   // spreading factor
   
	double Xt, Yt, Zt;		// location of transmitter
	double Xr, Yr, Zr;		// location of receiver

	t->getNode()->getLoc(&Xt, &Yt, &Zt);
	r->getNode()->getLoc(&Xr, &Yr, &Zr);

	double dX = Xr - Xt;
	double dY = Yr - Yt;
	double dZ = Zr - Zt;
	double d = sqrt(dX * dX + dY * dY + dZ * dZ);
	
	/*Added by Michael E. Zuba Fall 2012
	  Check for model parameter from tcl script
	  0 = Old Model, 1 = Roger's Model
	*/
	double useShallowModel = ifp->getShallowWaterModel();
	if (useShallowModel == 1) {
	/*Added by Michael E. Zuba Summer 2012
	  Obtain variable values for RogersModel
	  Convert meters to kilometers for RogersModel and check if d is less than 1km
	  Call RogersModel function to create transmission loss data
	*/
	double wd = ifp->getWaterDepth();
	double ssd = ifp->getSoundSpeedDiff();
	double sedtype = ifp->getSedimentType();

	double dkm = floor(d * 0.001);
	if(dkm == 0){dkm=1;}
	double TL = RogersModel(wd, ssd, F, sedtype, dkm);
        double Pr_db=171.5+10*log10(t->getTxPr());
	       Pr_db = Pr_db-TL;
        
        double Pr=pow(10,(0.1*(Pr_db-171.5)));
	//double Pr = t->getTxPr()/TL;
        
	printf("TL is new %g, old %g\n", (RogersModel(wd, ssd, F, sedtype, dkm)),(10*log10(Attenuation(d,K,F))));
        
	return Pr;				
	}
	else {
	// calculate receiving power at distance
	//double Pr = t->getTxPr()-(10*log10(Attenuation(d,K,F)));
	double Pr = t->getTxPr()/Attenuation(d,K,F);
	return Pr;
	}
	//return Pr;
}
//added by Yishan Su
double
UnderwaterPropagation::SNR(double Pr, UnderwaterPhy * ifp)
{        
       
        double F = ifp->getFrequency();	     // frequency range
	
        double Pr_db=171.5+10*log10(Pr);
        double shipping=1;               //fix now
        double wind=20;
        
	double SNR = Pr_db-Noise(F,shipping,wind);
        printf("UnderwaterPropagation::SNR is  %f, Pr is %f db,noise is %f\n",SNR, Pr_db,Noise(F,shipping,wind));
	return SNR;
}

double 
UnderwaterPropagation::Attenuation(double d, double k, double f)
{

  /* the distance unit used for absorption coefficient is km,  
     but for the attenuation, the used unit is meter 
   */

  double d1=d/1000.0; // convert to km 
  double t1=pow(d,k);
  double alpha_f=Thorp(f);
  double alpha=pow(10.0,(alpha_f/10.0));
  double t3=pow(alpha,d1);
  return t1*t3;
}

double
UnderwaterPropagation::Thorp(double f){
  return 0.11*((f*f)/(1.0+(f*f)))+44.0*((f*f)/(4100.0+(f*f)))
    +2.75*((f*f)/10000.0)+0.0003;
}


/* 	Transmission Loss Module for Shallow Water Environments.
 	Added by Michael E. Zuba and Yishan Su
 	
 	RogersModel function calculates Transmission Loss based on different sediments
 	from user/script inputs (i.e. water depth in meters (wd), sound speed difference (ssd),
 	water surface sound speed (cw -- currently hardcoded) and frequency in HZ (F).
 	sedtype is used to obtain TL based on sediment.
 	dist is used to obtain TL based on distance between S + R
*/
double 
UnderwaterPropagation::RogersModel(double _wd, double _ssd, double _F, double _sedtype, double _dist) {

	double cw = 1500;
	//Adjust distance for index use.
	_dist = _dist-1;
	_sedtype = _sedtype-1;
	
	double ratio[11];
	ratio[0] = 1.0040;
	ratio[1] = 1.0100;
	ratio[2] = 1.0200;
	ratio[3] = 1.0300;
	ratio[4] = 1.0400;
	ratio[5] = 1.0600;
	ratio[6] = 1.0800;
	ratio[7] = 1.1000;
	ratio[8] = 1.1200;
	ratio[9] = 1.2000;
	ratio[10] = 1.2500;
	
	double rho[11];
	rho[0] = 1.4100;
	rho[1] = 1.4900;
	rho[2] = 1.4400;
	rho[3] = 1.6100;
	rho[4] = 1.8100;
	rho[5] = 1.8200;
	rho[6] = 1.9500;
	rho[7] = 1.9700;
	rho[8] = 2;
	rho[9] = 2;
	rho[10] = 2.2000;
	
	double alpha[11];
	alpha[0] = 0.0300/1500;
	alpha[1] = 0.0300/1500;
	alpha[2] = 0.1500/1500;
	alpha[3] = 0.1100/1500;
	alpha[4] = 0.2000/1500;
	alpha[5] = 1.8900/1500;
	alpha[6] = 3/1500;
	alpha[7] = 2.7000/1500;
	alpha[8] = 2.5500/1500;
	alpha[9] = 2.4000/1500;
	alpha[10] = 2.4000/1500;
	
	double slop[11];
	slop[0] = 1;
	slop[1] = 1;
	slop[2] = 1.4000;
	slop[3] = 3.2000;
	slop[4] = 3.2000;
	slop[5] = 3.2000;
	slop[6] = 5;
	slop[7] = 5;
    	slop[8] = 5;
	slop[9] = 5;
	slop[10] = 5;
	
	double rhow = 1.03;
	double R, M, N, att, beta, alpw, theta, term;
	double tl;
	double f = _F*1000;

	R = (1000*_dist)+1000;
	M = rho[(int)_sedtype]/rhow;
	N = cw/(ratio[(int)_sedtype]*cw);
	att = alpha[(int)_sedtype]*f;
	term = N*att/((1-pow(N,2))*18.19);
	beta = 12.282*N*pow(((sqrt(1+term)-1)/((1-pow(N,2))*pow((1+pow(term,2)),2))),0.5);
	theta = max(sqrt(2*_ssd/cw), cw/(2*_F*_wd));
	alpw = 0.0010936*(0.1*pow(_F,2)/(1+pow(_F,2))+40*pow(_F,2)/(4100+pow(_F,2))+0.000275*pow(_F,2));
	tl = 15*log10(R)+5*log10(beta*_wd)+beta*R*pow(theta,2)/(4*_wd)+alpw*_dist-7.18;
	double tl_return = tl;

	return tl_return;
}

double 
UnderwaterPropagation::Noise(double F, double shipping, double wind) {

 	double turbulence = 17-30*log10(F);
	double ship = 40+20*(shipping-0.5)+26*log10(F)-60*log10(F+0.03);
	double win = 50+7.5*pow(wind,0.5)+20*log10(F)-40*log10(F+0.4);
	double thermal = -15+20*log10(F);

	double noise = turbulence+ship+win+thermal;
	
	return noise;
}
/*
double 
UnderwaterPropagation::RogersModel(double _wd, double _ssd, double _F, double _sedtype, double _dist) {

	double cw = 1500;
	//Adjust distance for index use.
	_dist = _dist-1;
	_sedtype = _sedtype-1;
	
	double ratio[11];
	ratio[0] = 1.0040;
	ratio[1] = 1.0100;
	ratio[2] = 1.0200;
	ratio[3] = 1.0300;
	ratio[4] = 1.0400;
	ratio[5] = 1.0600;
	ratio[6] = 1.0800;
	ratio[7] = 1.1000;
	ratio[8] = 1.1200;
	ratio[9] = 1.2000;
	ratio[10] = 1.2500;
	
	double rho[11];
	rho[0] = 1.4100;
	rho[1] = 1.4900;
	rho[2] = 1.4400;
	rho[3] = 1.6100;
	rho[4] = 1.8100;
	rho[5] = 1.8200;
	rho[6] = 1.9500;
	rho[7] = 1.9700;
	rho[8] = 2;
	rho[9] = 2;
	rho[10] = 2.2000;
	
	double alpha[11];
	alpha[0] = 0.0300/1500;
	alpha[1] = 0.0300/1500;
	alpha[2] = 0.1500/1500;
	alpha[3] = 0.1100/1500;
	alpha[4] = 0.2000/1500;
	alpha[5] = 1.8900/1500;
	alpha[6] = 3/1500;
	alpha[7] = 2.7000/1500;
	alpha[8] = 2.5500/1500;
	alpha[9] = 2.4000/1500;
	alpha[10] = 2.4000/1500;
	
	double slop[11];
	slop[0] = 1;
	slop[1] = 1;
	slop[2] = 1.4000;
	slop[3] = 3.2000;
	slop[4] = 3.2000;
	slop[5] = 3.2000;
	slop[6] = 5;
	slop[7] = 5;
    	slop[8] = 5;
	slop[9] = 5;
	slop[10] = 5;
	
	double rhow = 1.03;
	double R, M, N, att, beta, gra, alpw, theta;
	double tl;

	R = (1000*_dist)+1000;
	M = rho[(int)_sedtype]/rhow;
	N = cw/(ratio[(int)_sedtype]*cw);
	att = alpha[(int)_sedtype];
	beta = 0.477*M*N*att/pow(1-pow(N,2),1.5);
	gra = sqrt(1.7*_wd/(beta*R));
	theta = max(sqrt(2*_ssd/cw), cw/(2*_F*_wd));
	alpw = 0.001936*(0.1*pow(_F,2)/(1+pow(_F,2))+40*pow(_F,2)/(4100+pow(_F,2)));
	
	if (gra < theta){
		tl = 10*log10(R)+10*log10(_wd/(2*theta))+beta*R*pow(theta,2)/(4*_wd)+alpw*_dist;
	}
	else {
		tl = 15*log10(R)+5*log10(beta*_wd)+beta*R*pow(theta,2)/(4*_wd)+alpw*_dist-7.18;
	}

	double tl_return = tl;

	return tl_return;
}
*/
