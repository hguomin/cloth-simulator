
#include "spring.h"
#include "particle.h"

Spring::Spring() :
    m_particle1(nullptr),
    m_particle2(nullptr)
{
}

void Spring::Initialise(Particle& p1, Particle& p2)
{
    m_particle1 = &p1;
    m_particle2 = &p2;
    m_restDistance = (p1.GetPosition()-p2.GetPosition()).Length();
}

void Spring::SolveSpring()
{
    //current vector from p1 to p2
    FLOAT3 currentVector(m_particle2->GetPosition() - m_particle1->GetPosition());

    //current distance using vector
    float currentDistance = currentVector.Length();

    //'error' vector between p1 and p2 (need to minimise this)
    FLOAT3 errorVector(currentVector-((currentVector/currentDistance)*m_restDistance)); 

    //half of the error vector
    FLOAT3 errorVectorHalf(errorVector*0.5);
    
    //move the particles back into place
    m_particle1->MovePosition(errorVectorHalf);
    m_particle2->MovePosition(-errorVectorHalf);
}
