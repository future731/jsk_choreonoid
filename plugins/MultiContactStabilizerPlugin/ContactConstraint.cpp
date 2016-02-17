/**
   @author Kunio Kojima
*/

#include "ContactConstraint.h"

using namespace hrp;
using namespace std;

void StaticContactConstraintParam::calcInequalMatrix()
{
    inequalMat = dmatrix::Zero(numInequals,unitInputDim);
    for(std::vector<Vector3>::iterator iter = edgeVec.begin(); iter != edgeVec.end(); ++iter){
        int idx = std::distance(edgeVec.begin(), iter);
        inequalMat.block(idx,0, 1,6) << 0,0,(*iter)[2], (*iter)[1],-(*iter)[0],0;
    }
    inequalMat.block(edgeVec.size(),0, 4,3) << -1,0,mu, 0,-1,mu, 1,0,mu, 0,1,mu;
}

void StaticContactConstraintParam::calcInequalMinimumVector()
{
    inequalMinVec = dvector::Zero(numInequals);
}

void StaticContactConstraintParam::calcInequalMaximumVector()
{
    inequalMaxVec = dvector::Constant(numInequals,INFINITY);
}

void StaticContactConstraintParam::calcMinimumVector()
{
    minVec = dvector::Constant(unitInputDim,-INFINITY);
    minVec(2) = 0;
}

void StaticContactConstraintParam::calcMaximumVector()
{
    maxVec = dvector::Constant(unitInputDim,INFINITY);
}
