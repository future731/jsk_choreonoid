/**
   @author Kunio Kojima
*/
#include "MultiContactStabilizer.h"

using namespace hrp;
using namespace std;

MultiContactStabilizer::MultiContactStabilizer()
    : ModelPredictiveController<MultiContactStabilizer, MultiContactStabilizerParam>()
{
    unitInputDim = 6;// 接触点ごとの入力次元
    stateDim = 6;
}

void MultiContactStabilizer::setupQP()
{
    qpInterface = QP(URows, equalMatRows, inequalMatRows);

    // blockMatInv*(psiMat.transpose()*errorWeightMat*psiMat + inputWeightMat)*blockMat, blockMatInv*psiMat.transpose()*errorWeightMat*(phiMat*x0 - refXVec),
    // blockMatInv*(psiMat.transpose()*errorWeightMat*psiMat)*blockMat + blockMatInv*inputWeightMat*blockMat2, blockMatInv*psiMat.transpose()*errorWeightMat*(phiMat*x0 - refXVec),
    qpInterface.HMat = blockMat.transpose()*(psiMat.transpose()*errorWeightMat*psiMat)*blockMat + blockMatInv*inputWeightMat*blockMatInv.transpose();
    qpInterface.gVec = blockMat.transpose()*psiMat.transpose()*errorWeightMat*(phiMat*x0 - refXVec);
    // blockMat.transpose()*(psiMat.transpose()*errorWeightMat*psiMat + inputWeightMat)*blockMat, blockMat.transpose()*psiMat.transpose()*errorWeightMat*(phiMat*x0 - refXVec),// 1_6 2_7
    // blockMatInv*(psiMat.transpose()*errorWeightMat*psiMat + inputWeightMat)*blockMat2, blockMatInv*psiMat.transpose()*errorWeightMat*(phiMat*x0 - refXVec),


    qpInterface.equalMat = equalMat; qpInterface.equalVec = equalVec;
    qpInterface.inequalMat = inequalMat; qpInterface.inequalMinVec = inequalMinVec; qpInterface.inequalMaxVec = inequalMaxVec;
    qpInterface.minVec = minVec; qpInterface.maxVec = maxVec;
}

int MultiContactStabilizer::execQP()
{
    static int count = 0;
    if(COUT){
        cout << psiCols << " x " << URows << endl;
        cout << "psi:" << endl << psiMat << endl << endl;
        cout << "phi:" << endl << phiMat << endl << endl;
        cout << "refXVec:" << endl << refXVec.transpose() << endl << endl;
        cout << "x0:" << endl << x0.transpose() << endl << endl;
        cout << "W1(error):" << endl << errorWeightMat << endl << endl;
        cout << "W2(input):" << endl << inputWeightMat << endl << endl;
        cout << "block:" << endl << blockMat << endl << endl;
        cout << "blockInv:" << endl << blockMatInv << endl << endl;
    }
    int ret  = qpInterface.execQP(U);

    ++count;
    return ret;
}

void MultiContactStabilizerParam::calcSystemMatrix()
{
    double dt = controller()->dt;
    systemMat = dmatrix::Identity(stateDim,stateDim);
    const double m = controller()->m;
    double Fz = F(2);
    systemMat(0,1) = dt;
    systemMat(2,3) = dt;
    systemMat(4,2) = -dt*Fz/m;
    systemMat(5,0) = dt*Fz/m;
}

void MultiContactStabilizerParam::calcInputMatrix()
{
    double dt = controller()->dt;
    inputMat = dmatrix::Zero(stateDim,inputDim);
    for(std::vector<ContactConstraintParam*>::iterator iter = ccParamVec.begin(); iter != ccParamVec.end(); ++iter){
        int idx = std::distance(ccParamVec.begin(), iter);
        Matrix33 R = (*iter)->R;
        Vector3 p = (*iter)->p;

        dmatrix R2 = dmatrix::Zero(unitInputDim,unitInputDim);
        R2.block(0,0,3,3) = R;
        R2.block(3,3,3,3) = R;
        dmatrix unitInputMat = dmatrix(stateDim,unitInputDim);// B_k^i
        double T2_2 = dt*dt/2;
        double beta = dt*(p(2)-CM(2));
        unitInputMat <<
            T2_2,0, 0,        0, 0,0,
            dt,  0, 0,        0, 0,0,
            0,T2_2, 0,        0, 0,0,
            0,dt,   0,        0, 0,0,
            0,-beta,dt*p(1), dt,0,0,
            beta,0,-dt*p(0), 0,dt,0;
        inputMat.block(0,unitInputDim*idx, stateDim,unitInputDim) = unitInputMat*R2;
    }
}

void MultiContactStabilizerParam::calcMatrix(void (ContactConstraintParam::*calcFunc)(void))
{
    for(std::vector<ContactConstraintParam*>::iterator iter = ccParamVec.begin(); iter != ccParamVec.end(); ++iter){
        ((*iter)->*calcFunc)();
    }
}

void MultiContactStabilizerParam::calcVector(void (ContactConstraintParam::*calcFunc)(void))
{
    for(std::vector<ContactConstraintParam*>::iterator iter = ccParamVec.begin(); iter != ccParamVec.end(); ++iter){
        ((*iter)->*calcFunc)();
    }
}

void MultiContactStabilizerParam::calcEqualConstraints()
{
    int rowIdx = 0, colIdx = 0;
    equalMat = dmatrix::Zero(numEquals, inputDim);
    equalVec.resize(numEquals);

    // common constraint in different kinds of contact
    equalVec(rowIdx) = F(2);
    for(std::vector<ContactConstraintParam*>::iterator iter = ccParamVec.begin(); iter != ccParamVec.end(); ++iter){
        Matrix33 R = (*iter)->R;
        equalMat.block(rowIdx,colIdx, 1,3) = R.block(2,0,1,3);
        colIdx += unitInputDim;
    }
    ++rowIdx;

    // each contact constraint
    colIdx = 0;
    calcMatrix(&ContactConstraintParam::calcEqualMatrix);
    calcVector(&ContactConstraintParam::calcEqualVector);
    hrp::dumpMatrix(equalMat, &ContactConstraintParam::equalMat, ccParamVec, rowIdx, colIdx);
    hrp::dumpVector(equalVec, &ContactConstraintParam::equalVec, ccParamVec, rowIdx);
}

void MultiContactStabilizerParam::calcInequalConstraints()
{
    int rowIdx = 0, colIdx = 0;
    inequalMat = dmatrix::Zero(numInequals, inputDim);
    inequalMinVec.resize(numInequals);
    inequalMaxVec.resize(numInequals);
    calcMatrix(&ContactConstraintParam::calcInequalMatrix);
    calcVector(&ContactConstraintParam::calcInequalMinimumVector);
    calcVector(&ContactConstraintParam::calcInequalMaximumVector);
    hrp::dumpMatrix(inequalMat, &ContactConstraintParam::inequalMat, ccParamVec, rowIdx, colIdx);
    hrp::dumpVector(inequalMinVec, &ContactConstraintParam::inequalMinVec, ccParamVec, rowIdx);
    hrp::dumpVector(inequalMaxVec, &ContactConstraintParam::inequalMaxVec, ccParamVec, rowIdx);
}

void MultiContactStabilizerParam::calcBoundVectors()
{
    int rowIdx = 0;
    minVec.resize(inputDim);
    maxVec.resize(inputDim);
    calcVector(&ContactConstraintParam::calcMinimumVector);
    calcVector(&ContactConstraintParam::calcMaximumVector);
    hrp::dumpVector(minVec, &ContactConstraintParam::minVec, ccParamVec, rowIdx);
    hrp::dumpVector(maxVec, &ContactConstraintParam::maxVec, ccParamVec, rowIdx);
}

void MultiContactStabilizerParam::calcRefStateVector()
{
    const double m = controller()->m;
    dvector tmpVec(stateDim);
    tmpVec << m*CM(0),P(0), m*CM(1),P(1), L(0),L(1);
    setRefStateVector(tmpVec);
}

void MultiContactStabilizerParam::calcErrorWeightVector()
{
    errorWeightVec = dvector(stateDim);
    double errorCMWeight = controller()->errorCMWeight, errorMomentumWeight = controller()->errorMomentumWeight, errorAngularMomentumWeight = controller()->errorAngularMomentumWeight;
    errorWeightVec << errorCMWeight,errorMomentumWeight, errorCMWeight,errorMomentumWeight, errorAngularMomentumWeight,errorAngularMomentumWeight;
}

void MultiContactStabilizerParam::calcInputWeightMatrix()
{
    int rowIdx = 0, colIdx = 0;
    inputWeightMat = dmatrix::Zero(inputDim,inputDim);
    double inputForceWeight = controller()->inputForceWeight, inputMomentWeight = controller()->inputMomentWeight;
    for(std::vector<ContactConstraintParam*>::iterator iter = ccParamVec.begin(); iter != ccParamVec.end(); ++iter){
        int inputDim = (*iter)->inputDim;
        dvector tmpVec(6);
        tmpVec << inputForceWeight,inputForceWeight,inputForceWeight, inputMomentWeight,inputMomentWeight,inputMomentWeight;
        inputWeightMat.block(rowIdx,colIdx, inputDim,inputDim) = (*iter)->inputForceConvertMat.transpose()*tmpVec.asDiagonal()*(*iter)->inputForceConvertMat;
        rowIdx += inputDim;
        colIdx += inputDim;
    }
}

void MultiContactStabilizerParam::convertToMpcParam()
{
    inputDim = unitInputDim*ccParamVec.size();// 6*M_k
    numEquals = 1;//Fzの合計
    numInequals = 0;
    for(std::vector<ContactConstraintParam*>::iterator iter = ccParamVec.begin(); iter != ccParamVec.end(); ++iter){
        (*iter)->setupParam();
        numEquals += (*iter)->numEquals;
        numInequals += (*iter)->numInequals;
    }

    calcEqualConstraints();
    calcInequalConstraints();
    calcBoundVectors();

    calcInputMatrix();
    calcSystemMatrix();

    calcRefStateVector();

    calcErrorWeightVector();
    calcInputWeightMatrix();
}
