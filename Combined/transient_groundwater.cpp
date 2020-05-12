#include "../common/netcdf.hpp"
#include "ArrayPack.hpp"
#include "parameters.hpp"
#include <cassert>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <richdem/common/Array2D.hpp>
#include <richdem/common/timer.hpp>
#include <richdem/common/ProgressBar.hpp>
#include <stdexcept>
#include <string>
#include <vector>
#include <fstream>
using namespace std;

typedef std::vector<double> dvec;
typedef rd::Array2D<float>  f2d;

///////////////////////
// PRIVATE FUNCTIONS //
///////////////////////

double computeTransmissivity(int x, int y){
    if(arp.fdepth(x,y)>0){
        // Equation S6 from the Fan paper
        if(arp.wtd(x,y)<-1.5){
            return arp.fdepth(x,y) * arp.ksat(x,y) \
                       * std::exp( (arp.wtd(x,y)+1.5)/arp.fdepth(x,y) );
        }
        // If wtd is greater than 0, max out rate of groundwater movement
        // as though wtd were 0. The surface water will get to move in
        // FillSpillMerge.
        else if(arp.wtd(x,y) > 0){
            return arp.ksat(x,y) * (0+1.5+arp.fdepth(x,y));
        }
        //Equation S4 from the Fan paper
        else{
            return arp.ksat(x,y) * (arp.wtd(x,y) + 1.5 + arp.fdepth(x,y));
        }
    }
    // If the fdepth is zero, there is no water transmission below the surface
    // soil layer.
    // If it is less than zero, it is incorrect -- but no water transmission
    // also seems an okay thing to do in this case.
    else{
        return 0;
    }
}

void receiving_cell_wtd(float giving_cell_change, float giving_wtd,
                          float receiving_wtd, int x_giving, int y_giving,
                          int x_receiving, int y_receiving, ArrayPack &arp){
}


double get_change(const int x, const int y, const double time_remaining,
                  Parameters &params, ArrayPack &arp){

  // Declare variables updated in the loop
  double wtd_change_N;
  double wtd_change_S;
  double wtd_change_E;
  double wtd_change_W;

  // Elevation head - topography plus the water table depth (negative if
  // water table is below earth surface)
  const auto my_head = arp.topo(x,y) + params.me;
  // heads for each of my neighbour cells
  const auto headN   = arp.topo(x,y+1) + params.N;
  const auto headS   = arp.topo(x,y-1) + params.S;
  const auto headW   = arp.topo(x-1,y) + params.W;
  const auto headE   = arp.topo(x+1,y) + params.E;

  // Get the hydraulic conductivity for our cells of interest
  const auto kN = ( kcell(x, y) + kcell(x,  y+1) ) / 2.;
  const auto kS = ( kcell(x, y) + kcell(x,  y-1) ) / 2.;
  const auto kW = ( kcell(x, y) + kcell(x-1,y, ) ) / 2.;
  const auto kE = ( kcell(x, y) + kcell(x+1,y, ) ) / 2.;

  float mycell_change_N = 0.0;
  float mycell_change_S = 0.0;
  float mycell_change_E = 0.0;
  float mycell_change_W = 0.0;
  float mycell_change   = 0.0;

  float change_in_N_cell = 0.0;
  float change_in_S_cell = 0.0;
  float change_in_E_cell = 0.0;
  float change_in_W_cell = 0.0;

  bool stable = false;
  double time_step = time_remaining;

  //if( x== 1366&& y ==794)
  // std::cout<<"************************************"<<std::endl;

  while(!stable){

    //if( x== 1366&& y ==794)
    // std::cout<<"params S "<<params.S<<" wtd change S "<<wtd_change_S<<" change "<<change_in_S_cell<<" time "<<time_step<<" remaining "<<time_remaining<<std::endl;

    //   std::cout<<"time step is "<<time_step<<" and time remaining is "<<time_remaining<<std::endl;
    // Change in water-table depth.
    // (1) Discharge across cell boundaries
    // Average hydraulic conductivity of the two cells *
    // head difference between the two / distance (i.e., dH/dx_i) *
    // width of cell across which the water is discharged *
    // time step
    // (2) Divide by the area of the given cell: maps water volume
    // increases/decreases to change in head
    //Q_N = kN * (headN - my_head)
    //Q_S =
    //Q_W =
    //Q_E =


    wtd_change_N = kN * (headN - my_head) / params.cellsize_n_s_metres \
                    * arp.cellsize_e_w_metres[y] * time_step \
                    / arp.cell_area[y];
    wtd_change_S = kS * (headS - my_head) / params.cellsize_n_s_metres \
                    * arp.cellsize_e_w_metres[y] * time_step \
                    / arp.cell_area[y];
    wtd_change_E = kE * (headE - my_head) / arp.cellsize_e_w_metres[y] \
                    * params.cellsize_n_s_metres * time_step \
                    / arp.cell_area[y];
    wtd_change_W = kW * (headW - my_head) / arp.cellsize_e_w_metres[y] \
                    * params.cellsize_n_s_metres * time_step \
                    / arp.cell_area[y];


    //Using the wtd_changes from above, we need to calculate how much change will occur in the target cell, accounting for porosity.

    if(wtd_change_N > 1e-5){  //the current cell is receiving water from the North, so (x,y+1) is the giving cell.
      //target cell is the receiving cell.
      change_in_N_cell =  - wtd_change_N;
      mycell_change_N  = receiving_cell_wtd(wtd_change_N, params.N, params.me, x, y+1, x, y, arp);
    }
    else if (wtd_change_N < -1e-5){  //the current cell is giving water to the North. The North is the receiving cell.
      change_in_N_cell =  receiving_cell_wtd(-wtd_change_N, params.me, params.N, x, y, x, y+1, arp);
      mycell_change_N  = wtd_change_N;
    }
    else{
      wtd_change_N = 0;
      mycell_change_N = 0;
      change_in_N_cell = 0;
    }

    if(wtd_change_S > 1e-5){
      change_in_S_cell =  - wtd_change_S;
      mycell_change_S  = receiving_cell_wtd(wtd_change_S, params.S, params.me, x, y-1, x, y, arp);
    }
    else if (wtd_change_S < -1e-5){
      change_in_S_cell =  receiving_cell_wtd(-wtd_change_S, params.me, params.S, x, y, x, y-1, arp);
      mycell_change_S  = wtd_change_S;
    }
    else{
      wtd_change_S = 0;
      mycell_change_S = 0;
      change_in_S_cell = 0;
    }

    if(wtd_change_E > 1e-5){
      change_in_E_cell =  - wtd_change_E;
      mycell_change_E  = receiving_cell_wtd(wtd_change_E, params.E, params.me, x+1, y, x, y, arp);
    }
    else if (wtd_change_E < -1e-5){
      change_in_E_cell =  receiving_cell_wtd(-wtd_change_E, params.me, params.E, x, y, x+1, y, arp);
      mycell_change_E  = wtd_change_E;
    }
    else{
      wtd_change_E = 0;
      mycell_change_E = 0;
      change_in_E_cell = 0;
    }

    if(wtd_change_W > 1e-5){
      change_in_W_cell =  - wtd_change_W;
      mycell_change_W  = receiving_cell_wtd(wtd_change_W, params.W, params.me, x-1, y, x, y, arp);
    }
    else if (wtd_change_W < -1e-5){
      change_in_W_cell = receiving_cell_wtd(-wtd_change_W, params.me, params.W, x, y, x-1, y, arp);
      mycell_change_W  = wtd_change_W;
    }
    else{
      wtd_change_W = 0;
      mycell_change_W = 0;
      change_in_W_cell = 0;
    }

    //now we have the height changes that will take place in the target cell and each of the four neighbours.

    //Total change in wtd for our target cell in this iteration

    mycell_change =  mycell_change_N + mycell_change_E + mycell_change_S + mycell_change_W;

    if( ((headN > my_head) && (headS > my_head) && (change_in_N_cell+arp.topo(x,y+1)+params.N < mycell_change+arp.topo(x,y)+params.me) && (change_in_S_cell+arp.topo(x,y-1)+params.S < mycell_change+arp.topo(x,y)+params.me) && fabs(wtd_change_N)> 1e-6 && fabs(wtd_change_S) > 1e-6 ) ||  \
        ((headN < my_head) && (headS < my_head) && (change_in_N_cell+arp.topo(x,y+1)+params.N > mycell_change+arp.topo(x,y)+params.me) && (change_in_S_cell+arp.topo(x,y-1)+params.S > mycell_change+arp.topo(x,y)+params.me) && fabs(wtd_change_N)> 1e-6 && fabs(wtd_change_S) > 1e-6 ) ||  \
        ((headE > my_head) && (headW > my_head) && (change_in_E_cell+arp.topo(x+1,y)+params.E < mycell_change+arp.topo(x,y)+params.me) && (change_in_W_cell+arp.topo(x-1,y)+params.W < mycell_change+arp.topo(x,y)+params.me) && fabs(wtd_change_E)> 1e-6 && fabs(wtd_change_W) > 1e-6 ) ||  \
        ((headE < my_head) && (headW < my_head) && (change_in_E_cell+arp.topo(x+1,y)+params.E > mycell_change+arp.topo(x,y)+params.me) && (change_in_W_cell+arp.topo(x-1,y)+params.W > mycell_change+arp.topo(x,y)+params.me) && fabs(wtd_change_E)> 1e-6 && fabs(wtd_change_W) > 1e-6 )  ){
      //there is an instability.
      time_step = time_step/2.;
    }
    else if( (((headN - my_head)*((headN + change_in_N_cell) - (my_head + mycell_change_N)) < 0) && wtd_change_N > 1e-6)  || \
             (((headS - my_head)*((headS + change_in_S_cell) - (my_head + mycell_change_S)) < 0) && wtd_change_S > 1e-6) ||
             (((headE - my_head)*((headE + change_in_E_cell) - (my_head + mycell_change_E)) < 0) && wtd_change_E > 1e-6) ||
             (((headW - my_head)*((headW + change_in_W_cell) - (my_head + mycell_change_W)) < 0) && wtd_change_W > 1e-6) ){
      // The change between any 2 cells can't be greater than the difference
      // between those two cells.
      time_step = time_step/2.;
    }
    else{
      // Otherwise, a stable time step has been found
      arp.wtd_change_total(x,y) += ( mycell_change_N + mycell_change_E + mycell_change_S + mycell_change_W );
      params.me += mycell_change_N + mycell_change_E + mycell_change_S + mycell_change_W;
      params.N  += change_in_N_cell;
      params.S  += change_in_S_cell;
      params.W  += change_in_W_cell;
      params.E  += change_in_E_cell;

      stable = true;

      if(fabs(arp.wtd_change_total(x,y)) > 100000){
        std::cout<<"stable "<<arp.wtd(x,y)<<" change "<<arp.wtd_change_total(x,y)<<" x "<<x<<" y "<<y<<std::endl;

      std::cout<<"N "<<mycell_change_N<<" S "<<mycell_change_S<<" E "<<mycell_change_E<<" W "<<mycell_change_W<<std::endl;
      std::cout<<"wtd change N "<<wtd_change_N<<" S "<<wtd_change_S<<" E "<<wtd_change_E<<" W "<<wtd_change_W<<std::endl;
      std::cout<<"wtd W "<<arp.wtd(x-1,y)<<" wtd me "<<arp.wtd(x,y)<<std::endl;
      std::cout<<"W was the problem "<<headW<<" mine "<<my_head<<" k "<<kW<<" time step "<<time_step<<std::endl;

      //wtd_change_S = kS * (headS - my_head) / params.cellsize_n_s_metres \
                          * arp.cellsize_e_w_metres[y] * time_step \
                          / arp.cell_area[y];
      }
    }
  }
  return time_step;
}




void groundwater(Parameters &params, ArrayPack &arp){
  /**
  @param params   Global paramaters - we use the texfilename, run type,
                  number of cells in the x and y directions (ncells_x
                  and ncells_y), delta_t (number of seconds in a time step),
                  and cellsize_n_s_metres (size of a cell in the north-south
                  direction)

  @param arp      Global arrays - we access land_mask, topo, wtd,
                  wtd_change_total, cellsize_e_w_metres, and cell_area.
                  land_mask is a binary representation of where land is vs
                                        where ocean is.
                  topo is the input topography, i.e. land elevation above
                                        sea level in each cell.
                  wtd is the water table depth.
                  wtd_change_total is the amount by which wtd will change
                                        during this time step as a result of
                                        groundwater movement.
                  cellsize_e_w_metres is the distance across a cell in the
                                        east-west direction.
                  cell_area is the area of the cell, needed because different
                                        cells have different areas and
                                        therefore an accommodate different
                                        water volumes.

  @return  An updated wtd that represents the status of the water table after
           groundwater has been able to flow for the amount of time represented
           by delta_t.
  **/


  // Declare status variables and set initial values to 0
  double total_changes = 0.;
  float max_total      = 0.;
  float min_total      = 0.;
  float max_change     = 0.;


  // Set up log file
  ofstream textfile;
  textfile.open (params.textfilename, std::ios_base::app);

  textfile<<"Groundwater"<<std::endl;


  ////////////////////////////
  // COMPUTE CHANGES IN WTD //
  ////////////////////////////

  // Cycle through the entire array, calculating how much the water-table
  // changes in each cell per iteration.
  // We do this instead of using a staggered grid to approx. double CPU time
  // in exchange for using less memory.
  for(int y=1; y<params.ncells_y-1; y++){
    for(int x=1; x<params.ncells_x-1; x++){
      //skip ocean cells

      if(arp.land_mask(x,y) == 0)
        continue;

      double time_step = 0.0;
      arp.wtd_change_total(x,y) = 0.0;
      double time_remaining = params.deltat;

      params.N  = arp.wtd(x,y+1);
      params.S  = arp.wtd(x,y-1);
      params.E  = arp.wtd(x+1,y);
      params.W  = arp.wtd(x-1,y);
      params.me = arp.wtd(x,y);

      while(time_remaining > 1e-4){
        time_step = get_change(x, y,time_remaining,params,arp);
        time_remaining -= time_step;
      }
    }
  }


  ////////////////
  // UPDATE WTD //
  ////////////////

  for(int y=1;y<params.ncells_y-1;y++){
    for(int x=1;x<params.ncells_x-1; x++){

      if(arp.land_mask(x,y) == 0)
        continue;

      // Update the whole wtd array at once.
      // This is the new water table after groundwater has moved
      // for delta_t seconds.
      arp.wtd(x,y) += arp.wtd_change_total(x,y);
    }
  }

  // Write status to text file
  textfile << "total GW changes were " << total_changes << std::endl;
  textfile << "max wtd was " << max_total << " and min wtd was " \
           << min_total << std::endl;
  textfile << "max GW change was " << max_change << std::endl;
  textfile.close();
}