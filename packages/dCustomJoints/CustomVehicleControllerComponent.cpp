/* Copyright (c) <2009> <Newton Game Dynamics>
* 
* This software is provided 'as-is', without any express or implied
* warranty. In no event will the authors be held liable for any damages
* arising from the use of this software.
* 
* Permission is granted to anyone to use this software for any purpose,
* including commercial applications, and to alter it and redistribute it
* freely
*/


// NewtonCustomJoint.cpp: implementation of the NewtonCustomJoint class.
//
//////////////////////////////////////////////////////////////////////
#include <CustomJointLibraryStdAfx.h>
#include <CustomVehicleControllerJoint.h>
#include <CustomVehicleControllerManager.h>
#include <CustomVehicleControllerBodyState.h>
#include <CustomVehicleControllerComponent.h>


void CustomVehicleControllerComponent::dInterpolationCurve::InitalizeCurve (int points, const dFloat* const steps, const dFloat* const values)
{
	m_count = points;
	dAssert (points < int (sizeof(m_nodes)/sizeof (m_nodes[0])));
	memset (m_nodes, 0, sizeof (m_nodes));
	for (int i = 0; i < m_count; i ++) {
		m_nodes[i].m_param = steps[i];
		m_nodes[i].m_value = values[i];
	}
}

dFloat CustomVehicleControllerComponent::dInterpolationCurve::GetValue (dFloat param) const
{
	dAssert (m_count);
	param = dClamp (param, 0.0f, m_nodes[m_count - 1].m_param);
	dFloat interplatedValue = m_nodes[m_count - 1].m_value;
	for (int i = 1; i < m_count; i ++) {
		if (param < m_nodes[i].m_param) {
			dFloat df = m_nodes[i].m_value - m_nodes[i - 1].m_value;
			dFloat ds = m_nodes[i].m_param - m_nodes[i - 1].m_param;
			dFloat step = param - m_nodes[i - 1].m_param;

			interplatedValue = m_nodes[i - 1].m_value + df * step / ds;
			break;
		}
	}
	return interplatedValue;
}




CustomVehicleControllerComponentSteering::CustomVehicleControllerComponentSteering (CustomVehicleController* const controller, dFloat maxAngleInRadians)
	:CustomVehicleControllerComponent (controller)
	,m_maxAngle (dAbs (maxAngleInRadians))
{
}

void CustomVehicleControllerComponentSteering::AddSteeringTire (CustomVehicleControllerBodyStateTire* const tireNode, dFloat sign)
{
	dTireSignPair& pair = m_steeringTires.Append()->GetInfo();

	pair.m_sign = (sign >= 0.0f) ? 1.0f : -1.0f;
	pair.m_tireNode = m_controller->m_tireList.GetNodeFromInfo (*tireNode);
}


void CustomVehicleControllerComponentSteering::Update (dFloat timestep)
{
	for (dList<dTireSignPair>::dListNode* node = m_steeringTires.GetFirst(); node; node = node->GetNext()) {
		dTireSignPair& pair = node->GetInfo();
		CustomVehicleControllerBodyStateTire& tire = pair.m_tireNode->GetInfo();
		tire.m_steeringAngle = m_maxAngle * m_param * pair.m_sign;
	}
}


CustomVehicleControllerComponentBrake::CustomVehicleControllerComponentBrake (CustomVehicleController* const controller, dFloat maxBrakeTorque)
	:CustomVehicleControllerComponent (controller)
	,m_maxBrakeTorque (dAbs (maxBrakeTorque))
{
}

void CustomVehicleControllerComponentBrake::AddBrakeTire (CustomVehicleControllerBodyStateTire* const tire)
{
	m_brakeTires.Append(m_controller->m_tireList.GetNodeFromInfo (*tire));
}


void CustomVehicleControllerComponentBrake::Update (dFloat timestep)
{
	for (dList<dList<CustomVehicleControllerBodyStateTire>::dListNode*>::dListNode* node = m_brakeTires.GetFirst(); node; node = node->GetNext()) {
		CustomVehicleControllerBodyStateTire& tire = node->GetInfo()->GetInfo();
		tire.m_brakeTorque = dMax (tire.m_brakeTorque, dAbs (m_maxBrakeTorque * m_param));
	}
}


void CustomVehicleControllerComponentEngine::dDifferencial::ApplyTireTorque(dFloat shaftTorque, dFloat shaftOmega) const
{
	dFloat differentialGains[32];
	CustomVehicleControllerBodyStateTire* tireArray[32];

	GetTireArray (tireArray);
	int count = GetGainArray (differentialGains);

	dFloat gainSum = 0.0f;
	dFloat gainOmegaSum =  0.0f; 
	for (int i = 0; i < count; i ++) {
		gainSum += differentialGains[i];
		gainOmegaSum += dAbs(tireArray[i]->m_rotationalSpeed) * differentialGains[i];
	}

	if(dAbs (gainOmegaSum) < 1.0e-1f) {
		dFloat torqueContribution = shaftTorque / gainSum;
		for (int i = 0; i < count; i ++) {
			dFloat tireTorque = torqueContribution * differentialGains[i];
			const dMatrix& matrix = tireArray[i]->GetMatrix();
			const dVector& torque = tireArray[i]->GetTorque ();
			tireArray[i]->SetTorque(torque + matrix[0].Scale (tireTorque));
		}

	} else {
		dFloat shaftPower = shaftTorque * dAbs (shaftOmega) / gainOmegaSum;
		for (int i = 0; i < count; i ++) {
			dFloat tireTorque = shaftPower * differentialGains[i];
			const dMatrix& matrix = tireArray[i]->GetMatrix();
			const dVector& torque = tireArray[i]->GetTorque ();
			tireArray[i]->SetTorque(torque + matrix[0].Scale (tireTorque));
		}
	}
	
}

dFloat CustomVehicleControllerComponentEngine::dDifferencial::GetShaftOmega() const
{
	dFloat differentialGains[32];
	CustomVehicleControllerBodyStateTire* tireArray[32];

	GetTireArray (tireArray);
	int count = GetGainArray (differentialGains);

	dFloat gain = 0.0f;
	dFloat omega = 0.0f;
	
	for (int i = 0; i < count; i ++) {
		gain += differentialGains[i];
		omega += fabsf (tireArray[i]->m_rotationalSpeed) * differentialGains[i];
	}
	return omega / gain;
}


CustomVehicleControllerComponentEngine::dDifferencial::dDifferencial (CustomVehicleController* const controller)
	:m_gain0 (1.0f)
	,m_gain1 (1.0f)
{
}


CustomVehicleControllerComponentEngine::dTireDifferencial::dTireDifferencial (CustomVehicleController* const controller, CustomVehicleControllerBodyStateTire* const leftTire, CustomVehicleControllerBodyStateTire* const rightTire)
	:dDifferencial (controller)
	,m_tire0(leftTire)
	,m_tire1(rightTire)
{
	m_gain0 = 2.0f * m_tire0->m_radio / (m_tire0->m_radio + m_tire1->m_radio);
	m_gain1 = 2.0f - m_gain0;
}

int CustomVehicleControllerComponentEngine::dTireDifferencial::GetTireArray (CustomVehicleControllerBodyStateTire** const array) const
{
	array[0] = m_tire0;
	array[1] = m_tire1;
	return 2;
}


int CustomVehicleControllerComponentEngine::dTireDifferencial::GetGainArray (dFloat* const gains) const
{
	gains[0] = m_gain0;
	gains[1] = m_gain1;
	return 2;
}


CustomVehicleControllerComponentEngine::dEngineDifferencial::dEngineDifferencial (CustomVehicleController* const controller, dTireDifferencial* const frontDifferencial, dTireDifferencial* const rearDifferencial)
	:dDifferencial (controller)
	,m_differencial0 (frontDifferencial)
	,m_differencial1 (rearDifferencial)
{
	dFloat gainArray[4];
	CustomVehicleControllerBodyStateTire* tireArray[4];
	
	m_differencial0->GetTireArray(tireArray);
	m_differencial1->GetTireArray(&tireArray[2]);

	m_differencial0->GetGainArray(gainArray);
	m_differencial1->GetGainArray(&gainArray[2]);

	dFloat radio0 = tireArray[0]->m_radio / gainArray[0];
	dFloat radio1 = tireArray[2]->m_radio / gainArray[2];
	m_gain0 = 2.0f * radio0 / (radio0 + radio1);
	m_gain1 = 2.0f - m_gain0;
}

CustomVehicleControllerComponentEngine::dEngineDifferencial::~dEngineDifferencial()
{
	delete m_differencial0;
	delete m_differencial1;
}


int CustomVehicleControllerComponentEngine::dEngineDifferencial::GetGainArray (dFloat* const gains) const
{
	int count0 = m_differencial0->GetGainArray (gains);
	int count1 = m_differencial1->GetGainArray (&gains[count0]);
	return count0 + count1;
}

int CustomVehicleControllerComponentEngine::dEngineDifferencial::GetTireArray (CustomVehicleControllerBodyStateTire** const array) const
{
	int count0 = m_differencial0->GetTireArray (array);
	int count1 = m_differencial1->GetTireArray (&array[count0]);
	return count0 + count1;
}


CustomVehicleControllerComponentEngine::dGearBox::dGearBox(CustomVehicleController* const controller, dFloat reverseGearRatio, int gearCount, const dFloat* const gearBox)
	:m_gearsCount(gearCount + 2)
	,m_currentGear(NULL)
    ,m_controller(controller)
	,m_automatic(true)
{
	memset (m_gears, 0, sizeof (m_gears));

	dAssert (gearCount < (m_maxGears - 2));
	for (int i = 0; i < gearCount; i ++) {
        m_gears[i + m_firstGear] = new dGearState (gearBox[i], 0.9f, 0.3f, dGearID(i + m_firstGear)); 
	}

    for (int i = 0; i < gearCount - 1; i ++) {
        m_gears[i + m_firstGear]->m_next = m_gears[i + m_firstGear + 1]; 
    }
    

    for (int i = gearCount - 1; i; i --) {
        m_gears[i + m_firstGear]->m_prev = m_gears[i + m_firstGear - 1]; 
    }
 
    m_gears[m_reverseGear] = new dReverseGearState(reverseGearRatio);
    m_gears[m_newtralGear] = new dNeutralGearState(m_gears[m_firstGear], m_gears[m_reverseGear]);

    m_gears[m_firstGear]->m_prev = m_gears[m_firstGear];
    m_gears[m_firstGear + gearCount - 1]->m_next = m_gears[m_firstGear + gearCount - 1];

    m_currentGear = m_gears[m_newtralGear];

    for (int i = 0; i < m_gearsCount; i ++) {
        m_gears[i]->m_neutral = m_gears[m_newtralGear];
        m_gears[i]->m_reverse = m_gears[m_reverseGear];
    }
}

CustomVehicleControllerComponentEngine::dGearBox::~dGearBox ()
{
	for (int i = 0; i < m_gearsCount; i ++) {
		delete m_gears[i]; 
	}
}


void CustomVehicleControllerComponentEngine::dGearBox::SetOptimalShiftLimits (dFloat minShift, dFloat maxShift)
{
	minShift = dMax (minShift - 0.01f, 0.1f);
	maxShift = dMin (maxShift + 0.01f, 0.9f);
	for (int i = m_firstGear; i < m_gearsCount; i ++) {
		dGearState* const state = m_gears[i];
		state->m_shiftUp = maxShift;
		state->m_shiftDown = minShift;
	}
}

CustomVehicleControllerComponentEngine::dGearBox::dGearState* CustomVehicleControllerComponentEngine::dGearBox::dNeutralGearState::Update(CustomVehicleController* const vehicle)
{
    const CustomVehicleControllerComponentEngine* const engine = vehicle->GetEngine();
    dFloat param = engine->GetParam();

    if (param > dFloat (1.0e-3f)) {
        return m_next;
    } else if (param < dFloat(-1.0e-3f)) {
        return m_prev;
    }
    return this;
}

CustomVehicleControllerComponentEngine::dGearBox::dGearState* CustomVehicleControllerComponentEngine::dGearBox::dGearState::Update(CustomVehicleController* const vehicle)
{
    const CustomVehicleControllerComponentEngine* const engine = vehicle->GetEngine();

    dFloat param = engine->GetParam();
    dFloat speed = engine->GetSpeed();
    dFloat normalrpm = engine->GetRPM() / engine->GetRedLineRPM();

    if ((normalrpm > m_shiftUp) && (speed > 1.0f)) {
        return m_next;
    } else if (normalrpm < m_shiftDown) {
        if ((dAbs (speed) < 0.5f) && (param < dFloat (1.0e-3f))) {
            return m_neutral;
        }

        if ((dAbs (speed) < 2.0f) && (param < dFloat (-1.0e-3f))) {
            return m_reverse;
        }

        if (param > dFloat (-1.0e-3f)) 

        dAssert (m_prev != m_neutral);
        dAssert (m_prev != m_reverse);
        return m_prev;
    } else if (param < 0.0f) {
        dAssert (0);
/*
        if (speed < 1.0f) {
            return m_reverse;
        }
*/
    }

    return this;
}



dFloat CustomVehicleControllerComponentEngine::dGearBox::GetGearRatio(int gear) const 
{
	return m_gears[gear]->m_ratio;
}

void CustomVehicleControllerComponentEngine::dGearBox::SetTransmissionMode (bool mode) 
{
	m_automatic = mode;
}

bool CustomVehicleControllerComponentEngine::dGearBox::GetTransmissionMode () const 
{
	return m_automatic;
}


void CustomVehicleControllerComponentEngine::dGearBox::SetGear(int gear) 
{
    for (int i = 0; i < m_gearsCount; i ++) {
        if (m_gears[i]->m_id == gear) {
            m_currentGear = m_gears[i];
            break;
        }
    }
}

int CustomVehicleControllerComponentEngine::dGearBox::GetGear() const 
{
	return m_currentGear->m_id;
}

int CustomVehicleControllerComponentEngine::dGearBox::GetGearCount() const 
{
	return m_gearsCount;
}


void CustomVehicleControllerComponentEngine::dGearBox::Update(dFloat timestep)
{
	if (m_automatic) {
        m_currentGear = m_currentGear->Update(m_controller);
	}
}


CustomVehicleControllerComponentEngine::CustomVehicleControllerComponentEngine (CustomVehicleController* const controller, dGearBox* const gearBox, dDifferencial* const differencial)
	:CustomVehicleControllerComponent (controller)
	,m_gearBox(gearBox)
	,m_differencial (differencial)
	,m_speedMPS(0.0f)
	,m_engineRPS(0.0f)
	,m_topSpeedMPS(0.0f)
	,m_engineToque(0.0f)
	,m_crownGearRatio(1.0f)
	,m_engineIdleInvInertia(0.05f)
	,m_engineIdleResistance1(0.0f)
	,m_engineIdleResistance2(0.0f)
	,m_engineInternalFriction(0.0f)
	,m_clutchTorqueCouplingTime(0.25f)
	,m_radiansPerSecundsAtRedLine(0.0f)
	,m_radiansPerSecundsAtPeakPower(0.0f)
	,m_radiansPerSecundsAtIdleTorque(0.0f)
	,m_engineSwitch(false)
{
}

CustomVehicleControllerComponentEngine::~CustomVehicleControllerComponentEngine()
{
	if (m_differencial) {
		delete m_differencial;
	}
	if (m_gearBox) {
		delete m_gearBox;
	}
}



CustomVehicleControllerComponentEngine::dGearBox* CustomVehicleControllerComponentEngine::GetGearBox() const
{
    return m_gearBox;
}

bool CustomVehicleControllerComponentEngine::GetKey() const
{
	return m_engineSwitch;
}

void CustomVehicleControllerComponentEngine::SetKey (bool key)
{
	m_engineSwitch = key;
}


void CustomVehicleControllerComponentEngine::SetGear (int gear)
{
	m_gearBox->SetGear(gear);
}

int CustomVehicleControllerComponentEngine::GetGear () const
{
	return m_gearBox->GetGear();
}

void CustomVehicleControllerComponentEngine::SetTransmissionMode (bool mode)
{
	m_gearBox->SetTransmissionMode(mode);
}

bool CustomVehicleControllerComponentEngine::GetTransmissionMode () const
{
	return m_gearBox->GetTransmissionMode();
}

/*
dFloat CustomVehicleControllerComponentEngine::GetRedLineResistance() const
{
    return m_engineResistance;
}

dFloat CustomVehicleControllerComponentEngine::GetIdleResistance() const
{
    return m_engineIdleResistance;
}


dFloat CustomVehicleControllerComponentEngine::GetIdleRadianPerSeconds () const
{
	return m_radiansPerSecundsAtIdleTorque;
}

dFloat CustomVehicleControllerComponentEngine::GetDifferencialGearRatio() const
{
	dAssert(0);
	return m_crownGearRatio;
}
*/

dFloat CustomVehicleControllerComponentEngine::GetTorque (dFloat radianPerSeconds) const
{
    return m_torqueCurve.GetValue(radianPerSeconds);
}

/*
dList<CustomVehicleControllerBodyStateTire>::dListNode* CustomVehicleControllerComponentEngine::GetLeftTireNode() const
{
    return m_leftTire;
}

dList<CustomVehicleControllerBodyStateTire>::dListNode* CustomVehicleControllerComponentEngine::GetRightTireNode() const
{
    return m_righTire;
}
*/

dFloat CustomVehicleControllerComponentEngine::GetTopSpeed () const
{
	return m_topSpeedMPS;
}

dFloat CustomVehicleControllerComponentEngine::GetSpeed () const
{
	return m_speedMPS;
}

dFloat CustomVehicleControllerComponentEngine::GetRPM () const
{
	return m_engineRPS * m_crownGearRatio * 9.55f;
}

dFloat CustomVehicleControllerComponentEngine::GetTopRPM () const
{
    return m_radiansPerSecundsAtPeakPower * m_crownGearRatio * 9.55f;
}

dFloat CustomVehicleControllerComponentEngine::GetRedLineRPM() const 
{
	return m_radiansPerSecundsAtRedLine * m_crownGearRatio * 9.55f;
}

void CustomVehicleControllerComponentEngine::SetTopSpeed (dFloat topSpeedMPS, dFloat rpsAtPickPower)
{
	dAssert (topSpeedMPS >= 0.0f);
	dAssert (topSpeedMPS < 100.0f);

	dFloat differentialGains[32];
	CustomVehicleControllerBodyStateTire* tireArray[32];

	m_differencial->GetTireArray (tireArray);
	m_differencial->GetGainArray(differentialGains);

	CustomVehicleControllerBodyStateTire* const tire = tireArray[0];
	dAssert (tire);
	m_topSpeedMPS = topSpeedMPS;
	dFloat effectiveRadio = tire->m_radio / differentialGains[0];

	// drive train geometrical relations
	// G0 = m_differentialGearRatio
	// G1 = m_transmissionGearRatio
	// s = topSpeedMPS
	// r = tireRatio
	// wt = rpsAtTire
	// we = rpsAtPickPower
	// we = G1 * G0 * wt;
	// wt = e / r
	// we = G0 * G1 * s / r
	// G0 = r * we / (G1 * s)
	// using the top gear and the optimal engine torque for the calculations
	dFloat topGearRatio = m_gearBox->GetGearRatio(m_gearBox->GetGearCount() - 1);
	m_crownGearRatio = effectiveRadio * rpsAtPickPower / (topSpeedMPS * topGearRatio);
}





void CustomVehicleControllerComponentEngine::ConvertToMetricSystem (dFloat& vehicleSpeedInKilometerPerHours,
	dFloat& idleTorqueInPoundFoot, dFloat& revolutionsPerMinutesAtIdleTorque, 
	dFloat& peakTorqueInPoundFoot, dFloat& revolutionsPerMinutesAtPeakTorque, 
	dFloat& peakHorsePower, dFloat& revolutionsPerMinutesAtPeakHorsePower, 
	dFloat& torqueAtRedLineInPoundFoot, dFloat& revolutionsPerMinutesAtRedLineTorque) const
{
	const dFloat horsePowerToWatts = 735.5f;
	const dFloat kmhToMetersPerSecunds = 0.278f;
	const dFloat rpmToRadiansPerSecunds = 0.105f;
	const dFloat poundFootToNewtonMeters = 1.356f;
		
	peakHorsePower *= horsePowerToWatts;
	idleTorqueInPoundFoot *= poundFootToNewtonMeters;
	peakTorqueInPoundFoot *= poundFootToNewtonMeters;
	torqueAtRedLineInPoundFoot *= poundFootToNewtonMeters;

	revolutionsPerMinutesAtIdleTorque *= rpmToRadiansPerSecunds;
	revolutionsPerMinutesAtPeakTorque *= rpmToRadiansPerSecunds;
	revolutionsPerMinutesAtPeakHorsePower *= rpmToRadiansPerSecunds;
	revolutionsPerMinutesAtRedLineTorque *= rpmToRadiansPerSecunds;

	vehicleSpeedInKilometerPerHours *= kmhToMetersPerSecunds;
}


void CustomVehicleControllerComponentEngine::InitEngineTorqueCurve (dFloat vehicleSpeed,
	dFloat idleTorque, dFloat rpsAtIdleTorque, 
	dFloat peakTorque, dFloat rpsAtPeakTorque, 
	dFloat peakHorsePower, dFloat rpsAtPeakHorsePower, 
	dFloat torqueAtRedLine, dFloat rpsAtRedLineTorque)
{
	int oldGear = m_gearBox->GetGear();
	m_gearBox->SetGear(m_gearBox->GetGearCount() - 1);

	ConvertToMetricSystem (vehicleSpeed, idleTorque, rpsAtIdleTorque, peakTorque, rpsAtPeakTorque, peakHorsePower, rpsAtPeakHorsePower, torqueAtRedLine, rpsAtRedLineTorque);
	SetTopSpeed (vehicleSpeed, rpsAtPeakHorsePower);				  	   

	dFloat torqueAtPeakPower = peakHorsePower / rpsAtPeakHorsePower;
	dAssert (rpsAtIdleTorque > 0.0f);
	dAssert (rpsAtIdleTorque < rpsAtPeakTorque);
	dAssert (rpsAtPeakTorque < rpsAtPeakHorsePower);
	dAssert (rpsAtPeakHorsePower < rpsAtRedLineTorque);

	dAssert (idleTorque > 0.0f);
	dAssert (idleTorque < peakTorque);
	dAssert (peakTorque > torqueAtPeakPower);
	dAssert (torqueAtPeakPower > torqueAtRedLine);
	dAssert (torqueAtRedLine > 0.0f);
	dAssert (peakTorque * rpsAtPeakTorque < peakHorsePower);

	dFloat rpsTable[5];
	dFloat torqueTable[5];

	rpsTable[0] = 0.0f;
	rpsTable[1] = rpsAtIdleTorque;
	rpsTable[2] = rpsAtPeakTorque;
	rpsTable[3] = rpsAtPeakHorsePower;
	rpsTable[4] = rpsAtRedLineTorque;

	torqueTable[0] = idleTorque;
	torqueTable[1] = idleTorque;
	torqueTable[2] = peakTorque;
	torqueTable[3] = torqueAtPeakPower;
	torqueTable[4] = torqueAtRedLine;

	const int count = sizeof (rpsTable) / sizeof (rpsTable[0]);
	for (int i = 0; i < count; i ++) {
		rpsTable[i] /= m_crownGearRatio;
		torqueTable[i] *= m_crownGearRatio * 1.0f;
	}
	m_torqueCurve.InitalizeCurve (sizeof (rpsTable)/sizeof (rpsTable[0]), rpsTable, torqueTable);

	m_radiansPerSecundsAtIdleTorque = rpsTable[1];
	m_radiansPerSecundsAtPeakPower = rpsTable[3];
	m_radiansPerSecundsAtRedLine = rpsTable[4];
	m_engineInternalFriction = torqueTable[2] / (m_radiansPerSecundsAtRedLine * 4.0f);

	m_engineIdleResistance1 = torqueTable[2] / (m_radiansPerSecundsAtRedLine * 2.0f);
	dFloat W = 0.5f * (rpsTable[3] + rpsTable[4]);
	dFloat T = GetTorque (W);
	m_engineIdleResistance2 = (T - W * m_engineIdleResistance1) / (W * W);
	dAssert (m_engineIdleResistance2 > 0.0f);

	m_gearBox->SetOptimalShiftLimits (m_radiansPerSecundsAtIdleTorque / m_radiansPerSecundsAtRedLine, m_radiansPerSecundsAtPeakPower/ m_radiansPerSecundsAtRedLine);
	m_gearBox->SetGear(oldGear);
}


void CustomVehicleControllerComponentEngine::Update (dFloat timestep)
{
	CustomVehicleControllerBodyStateChassis& chassis = m_controller->m_chassisState;
	CustomVehicleControllerComponentEngine::dGearBox* const gearBox = GetGearBox();

	gearBox->Update (timestep);

	int gear = gearBox->GetGear();
if (gear > (CustomVehicleControllerComponentEngine::dGearBox::m_firstGear + 1))
{
//	gearBox->SetGear (CustomVehicleControllerComponentEngine::dGearBox::m_firstGear + 1);
//	gear = gearBox->GetGear();
}

	
	if (m_engineSwitch) {
		dFloat gearRatio = gearBox->GetGearRatio(gear);
		dFloat shaftOmega = m_differencial->GetShaftOmega();
		if (gear == CustomVehicleControllerComponentEngine::dGearBox::m_newtralGear) {
			dFloat param = dMax (GetParam(), 0.05f);
			m_engineToque = GetTorque (m_engineRPS) * param - m_engineIdleResistance1 * m_engineRPS - m_engineIdleResistance2 * m_engineRPS * m_engineRPS;
			dFloat alpha = m_engineIdleInvInertia * m_engineToque;
			m_engineRPS = dMin (dMax (m_engineRPS + alpha * timestep, 0.0f), m_radiansPerSecundsAtRedLine);
		} else {
			dFloat param = GetParam() * 0.5f;
			m_engineRPS = shaftOmega * gearRatio;
			dFloat nominalTorque = GetTorque (dAbs (m_engineRPS)) * param - m_engineInternalFriction * m_engineRPS;
			m_engineToque = m_engineToque + (nominalTorque - m_engineToque) * timestep / m_clutchTorqueCouplingTime;

			dFloat shaftTorque = -m_engineToque * gearRatio;
			m_differencial->ApplyTireTorque(shaftTorque, shaftOmega);

			/*
			// apply the engine torque on this tire
			dFloat sign = dSign (gearRatio);
			dVector reactionTorque (chassis.m_matrix[2].Scale (m_engineToque * sign * 0.25f));

			// there is not direct joint between the tire and the chassis, but there tire inertia should apply some toque to the engine
			// I will use an arbitrary 25% of the torque goes to the chassis.
			chassis.SetTorque(chassis.GetTorque() + reactionTorque);
			*/
		}
	} else {
		m_engineRPS = m_engineRPS * 0.95f;
		if (m_engineRPS < 0.001f) {
			m_engineRPS = 0.0f;
		}
		m_engineToque = 0.0f;
	}

	


	dVector front (chassis.m_matrix.RotateVector(chassis.m_localFrame[0]));
	m_speedMPS = chassis.m_veloc % front;
}
