////////////////////////////////////////////////////////////////////////////////////////
// Kara Jensen - mail@karajensen.com - spring.cpp
////////////////////////////////////////////////////////////////////////////////////////

#include "spring.h"
#include "particle.h"
#include "diagnostic.h"

Spring::Spring() :
    m_type(STRETCH),
    m_id(NO_INDEX),
    m_color(0),
    m_particle1(nullptr),
    m_particle2(nullptr),
    m_restDistance(0.0f)
{
}

void Spring::Initialise(Particle& p1, Particle& p2, int id, Type type)
{
    m_type = type;
    m_id = id;
    m_particle1 = &p1;
    m_particle2 = &p2;
   
    D3DXVECTOR3 difference = p1.GetPosition()-p2.GetPosition();
    m_restDistance = D3DXVec3Length(&difference);

    switch(type)
    {
    case STRETCH:
        m_color = Diagnostic::RED;
        break;
    case SHEAR:
        m_color = Diagnostic::GREEN;
        break;
    case BEND:
        m_color = Diagnostic::YELLOW;
        break;
    }
}

void Spring::SolveSpring()
{
    //current vector from p1 to p2
    D3DXVECTOR3 currentVector(m_particle2->GetPosition() - m_particle1->GetPosition());

    //current distance using vector
    float currentDistance = D3DXVec3Length(&currentVector);

    //'error' vector between p1 and p2 (need to minimise this)
    D3DXVECTOR3 errorVector(currentVector-((currentVector/currentDistance)*m_restDistance)); 

    //half of the error vector
    D3DXVECTOR3 errorVectorHalf(errorVector*0.5);
    
    //move the particles back into place
    m_particle1->MovePosition(errorVectorHalf);
    m_particle2->MovePosition(-errorVectorHalf);
}

void Spring::UpdateDiagnostic(Diagnostic* diagnostic) const
{
    if(m_type == BEND)
    {
        diagnostic->UpdateSphere(Diagnostic::CLOTH, "Spring"+StringCast(m_id), 
            static_cast<Diagnostic::Colour>(m_color),
            (m_particle1->GetPosition()*0.5f)+(m_particle2->GetPosition()*0.5f), 0.1f);
    }
    else
    {
        diagnostic->UpdateLine(Diagnostic::CLOTH, "Spring"+StringCast(m_id), 
            static_cast<Diagnostic::Colour>(m_color),
            m_particle1->GetPosition(), m_particle2->GetPosition());
    }
}
