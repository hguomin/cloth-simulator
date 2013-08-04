#include "meshmanager.h"
#include "mesh.h"
#include "cloth.h"
#include "picking.h"

namespace
{
    const int MAX_INSTANCES = 20;
    const int GROUND_INDEX = 0;
    const std::string MODEL_FOLDER(".\\Resources\\Models\\");
}

MeshManager::MeshManager(LPDIRECT3DDEVICE9 d3ddev, std::shared_ptr<Shader> meshshader) :
    m_selectedTool(NONE),
    m_d3ddev(d3ddev),
    m_selectedMesh(NO_INDEX)
{
    m_templates.resize(MAX_OBJECT);
    m_meshes.resize(MAX_INSTANCES);
    std::generate(m_meshes.begin(), m_meshes.end(), [&](){ return MeshManager::MeshPtr(new Mesh()); });
    std::generate(m_templates.begin(), m_templates.end(), [&](){ return MeshManager::MeshPtr(new Mesh()); });

    // fill in the instance queue
    for(unsigned int i = 1; i < m_meshes.size(); ++i)
    {
        m_open.push(i);
    }

    auto createMesh = [&](const std::string& name, MeshPtr& mesh)
    {
        if(!mesh->Load(d3ddev, MODEL_FOLDER + name, meshshader))
        {
            Diagnostic::ShowMessage(name + " failed to load");
        }
    };

    // Create the sphere template
    createMesh("sphere.obj", m_templates[SPHERE]);
    m_templates[SPHERE]->SetVisible(false);
    m_templates[SPHERE]->CreateCollision(d3ddev, 0.5f, 8);

    // Create the box template
    createMesh("box.obj", m_templates[BOX]);
    m_templates[BOX]->SetVisible(false);
    m_templates[BOX]->CreateCollision(d3ddev, 5.0f, 5.0f, 5.0f);

    // Create the cylinder template
    createMesh("cylinder.obj", m_templates[CYLINDER]);
    m_templates[CYLINDER]->SetVisible(false);

    // Create the ground plain
    createMesh("ground.obj", m_meshes[GROUND_INDEX]);
    m_meshes[GROUND_INDEX]->SetPickable(false);
    m_meshes[GROUND_INDEX]->SetPosition(0,-20,0);
    m_meshes[GROUND_INDEX]->CreateCollision(d3ddev,150.0f,1.0f,150.0f);
}

void MeshManager::ChangeTool(Tool tool)
{
    m_selectedTool = tool;
}

bool MeshManager::AddObject(Object object)
{
    if(m_open.empty())
    {
        return false;
    }

    unsigned int index = m_open.front();
    m_open.pop();

    switch(object)
    {
    case BOX:
        break;
    case SPHERE:
        break;
    case CYLINDER:
        break;
    }

    return true;
}

void MeshManager::RemoveScene()
{
    m_selectedMesh = NO_INDEX;
}

void MeshManager::RemoveObject()
{
    if(m_selectedMesh != NO_INDEX)
    {
        m_open.push(m_selectedMesh);
    }
}

void MeshManager::Draw(const D3DXVECTOR3& position, const Transform& projection, const Transform& view)
{
    std::for_each(m_meshes.begin(), m_meshes.end(), [&](MeshPtr& mesh)
    {
        if(mesh.get() && mesh->IsVisible())
        {
            mesh->DrawMesh(position, projection, view);
        }
    });
}

void MeshManager::DrawCollision(const Transform& projection, const Transform& view)
{
    std::for_each(m_meshes.begin(), m_meshes.end(), [&](MeshPtr& mesh)
    {
        if(mesh.get() && mesh->IsVisible())
        {
            mesh->DrawCollision(projection, view);
        }
    });
}

void MeshManager::MousePickingTest(Picking& input)
{
    std::for_each(m_meshes.begin(), m_meshes.end(), [&](MeshPtr& mesh)
    {
        if(mesh.get() && mesh->IsVisible())
        {
            mesh->MousePickingTest(input);
        }
    });
}

void MeshManager::SolveClothCollision(Cloth& cloth)
{
    std::for_each(m_meshes.begin(), m_meshes.end(), [&](MeshPtr& mesh)
    {
        if(mesh.get() && mesh->IsVisible())
        {
            cloth.SolveCollision(mesh->GetCollision());
        }
    });
}

void MeshManager::SetCollisionVisibility(bool visible)
{
    std::for_each(m_meshes.begin(), m_meshes.end(), [&](MeshPtr& mesh)
    {
        if(mesh.get())
        {
            mesh->SetCollisionVisibility(visible);
        }
    });
}