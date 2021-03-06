#if defined(__GNUC__)
#ident "University of Edinburgh $Id$"
#else
static char _MAPMSnake_c[] = "University of Edinburgh $Id$";
#endif
/*!
* \file         MAPMSnake.c
* \author	Richard Baldock
* \date		April 2009
* \version      $Id$
* \par
* Address:
*               MRC Human Genetics Unit,
*               MRC Institute of Genetics and Molecular Medicine,
*               University of Edinburgh,
*               Western General Hospital,
*               Edinburgh, EH4 2XU, UK.
* \par
* Copyright (C), [2012],
* The University Court of the University of Edinburgh,
* Old College, Edinburgh, UK.
* 
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be
* useful but WITHOUT ANY WARRANTY; without even the implied
* warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
* PURPOSE.  See the GNU General Public License for more
* details.
*
* You should have received a copy of the GNU General Public
* License along with this program; if not, write to the Free
* Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
* Boston, MA  02110-1301, USA.
* \ingroup      MAPaint
* \brief        
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <MAPaint.h>

static PMSnakeSearchParams	staticSearchParams;
static PMSnakeNLCParams		staticNLCParams;
static PMEdgeSnakeNLCParams	staticEdgeNLCParams;
static double			**staticDirection;

void PMSnakeNlcSetup(
  PMSnakeNLCParams	*params)
{
  staticNLCParams = *params;

  return;
}

void PMEdgeSnakeNlcSetup(
  PMEdgeSnakeNLCParams	*params,
  double	**direction)
{
  staticEdgeNLCParams = *params;
  staticDirection = direction;

  return;
}

double PMSnakeNlc(
  int	i,
  int	j,
  int	jp,
  int	**op)
{
  double	cost, alpha, kappa;

  /* angle cost */
  alpha = (double) (j - jp) / staticSearchParams.spacing;
  cost = staticNLCParams.nu_alpha * alpha * alpha;

  /* curvature cost */
  kappa = 0.0;
  if( i > 0 )
  {
    kappa = (double) (j - 2*jp + op[i-1][jp]) / staticSearchParams.spacing /
      staticSearchParams.spacing;
  }
  cost += staticNLCParams.nu_kappa * kappa * kappa;

  return( cost );
}
double PMEdgeSnakeNlc(
  int	i,
  int	j,
  int	jp,
  int	**op)
{
  double	cost, alpha, kappa, dir;

  /* angle cost */
  alpha = (double) (j - jp) / staticSearchParams.spacing;
  cost = staticEdgeNLCParams.snakeParams->nu_alpha * alpha * alpha;

  /* curvature cost */
  kappa = 0.0;
  if( i > 0 )
  {
    kappa = (double) (j - 2*jp + op[i-1][jp]) / staticSearchParams.spacing /
      staticSearchParams.spacing;
  }
  cost += staticEdgeNLCParams.snakeParams->nu_kappa * kappa * kappa;

  /* direction cost */
  dir = 0.0;
  if( i > 0 ){
    dir = fabs(staticDirection[i][j] - staticDirection[i-1][jp]) / WLZ_M_PI;
    dir -= floor(dir);
  }
  cost += staticEdgeNLCParams.nu_direction * dir * dir;

  return( cost );
}


void PMSnake(
  double		**local_cost,
  int			num_mpts,
  MATrackMatchPointStruct	*mpts,
  PMSnakeSearchParams	*searchParams,
  PMSnakeLCParams	*LCParams,
  PMSnakeNLCParams	*NLCParams,
  int			closed_path)
{
  double	**optimal_cost;
  int		**optimal_path;
  int		i, j, opt_j;
  double	dist_factor;
  double	min_cost;

  (void) AlcDouble2Malloc(&optimal_cost, num_mpts, 2*searchParams->range+1);
  (void) AlcInt2Malloc(&optimal_path, num_mpts, 2*searchParams->range+1);

  /* add in the local distance cost - log gaussian probability
     ie quadratic */
  dist_factor = LCParams->nu_dist / searchParams->range / searchParams->range;
  for(i=0; i < num_mpts; i++)
  {
    for(j = -searchParams->range; j <= searchParams->range; j++)
    {
      local_cost[i][j+searchParams->range] += dist_factor * j * j;
    }
  }

  /* set up the parameters for the non-local cost */
  staticSearchParams = *searchParams;
  PMSnakeNlcSetup(NLCParams);

  /* call the dynamic programming search */
  (void) AlgDPSearch(num_mpts, 2*searchParams->range+1,
		       local_cost, optimal_cost,
		       optimal_path, PMSnakeNlc);

  /* extract the optimal path and calculate offsets */
  opt_j = 0;
  min_cost = optimal_cost[num_mpts-1][opt_j];
  for(j = -searchParams->range; j <= searchParams->range; j++)
  {
    if( optimal_cost[num_mpts-1][j+searchParams->range] < min_cost )
    {
      min_cost = optimal_cost[num_mpts-1][j+searchParams->range];
      opt_j = j + searchParams->range;
    }
  }
  for(i=num_mpts-1; i >= 0; i--)
  {
    mpts[i].x_off = WLZ_NINT((opt_j - searchParams->range) *
			     mpts[i].costheta);
    mpts[i].y_off = WLZ_NINT((opt_j - searchParams->range) *
			     mpts[i].sintheta);

    opt_j = optimal_path[i][opt_j];
  }

  /* clean up memory allocation */
  AlcDouble2Free(optimal_cost);
  AlcInt2Free(optimal_path);
  return;
}

void PMEdgeSnake(
  double		**local_cost,
  double		**direction,
  int			num_mpts,
  MATrackMatchPointStruct	*mpts,
  PMSnakeSearchParams	*searchParams,
  PMSnakeLCParams	*LCParams,
  PMEdgeSnakeNLCParams	*NLCParams,
  int			closed_path)
{
  double	**optimal_cost;
  int		**optimal_path;
  int		i, j, opt_j;
  double	dist_factor;
  double	min_cost;

  (void) AlcDouble2Malloc(&optimal_cost, num_mpts, 2*searchParams->range+1);
  (void) AlcInt2Malloc(&optimal_path, num_mpts, 2*searchParams->range+1);

  /* write out the local cost values */
  fprintf(stderr, "local_cost before distance cost\n");
  for(i=0; i < num_mpts; i++){
    for(j = -searchParams->range; j <= searchParams->range; j++){
      fprintf(stderr, "%7.4f, ", local_cost[i][j+searchParams->range]);
    }
    fprintf(stderr, "\n");
  }

  /* add in the local distance cost - log gaussian probability
     ie quadratic */
  dist_factor = LCParams->nu_dist / searchParams->range / searchParams->range;
  for(i=0; i < num_mpts; i++)
  {
    for(j = -searchParams->range; j <= searchParams->range; j++)
    {
      local_cost[i][j+searchParams->range] += dist_factor * j * j;
    }
  }

  /* write out the local cost values */
  fprintf(stderr, "local_cost after distance cost\n");
  for(i=0; i < num_mpts; i++){
    for(j = -searchParams->range; j <= searchParams->range; j++){
      fprintf(stderr, "%7.4f, ", local_cost[i][j+searchParams->range]);
    }
    fprintf(stderr, "\n");
  }

  /* set up the parameters for the non-local cost */
  staticSearchParams = *searchParams;
  PMEdgeSnakeNlcSetup(NLCParams, direction);

  /* call the dynamic programming search */
  (void) AlgDPSearch(num_mpts, 2*searchParams->range+1,
		       local_cost, optimal_cost,
		       optimal_path, PMSnakeNlc);

  /* extract the optimal path and calculate offsets */
  opt_j = 0;
  min_cost = optimal_cost[num_mpts-1][opt_j];
  for(j = -searchParams->range; j <= searchParams->range; j++)
  {
    if( optimal_cost[num_mpts-1][j+searchParams->range] < min_cost )
    {
      min_cost = optimal_cost[num_mpts-1][j+searchParams->range];
      opt_j = j+searchParams->range;
    }
  }
  for(i=num_mpts-1; i >= 0; i--)
  {
    mpts[i].x_off = WLZ_NINT((opt_j - searchParams->range) * mpts[i].costheta);
    mpts[i].y_off = WLZ_NINT((opt_j - searchParams->range) * mpts[i].sintheta);

    opt_j = optimal_path[i][opt_j];
  }

  /* clean up memory allocation */
  AlcDouble2Free(optimal_cost);
  AlcInt2Free(optimal_path);
  return;
}

