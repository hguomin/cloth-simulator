#include "meshmanager.h"
#include "mesh.h"
#include "cloth.h"
#include "picking.h"
#include "diagnostic.h"

namespace
{
    const int MAX_INSTANCES = 6;
    const int GROUND_INDEX = 0;
    const std::string MODEL_FOLDER(".\\Resources\\Models\\");
    const std::string TEXTURE_FOLDER(".\\Resources\\Textures\\");
}

MeshManager::MeshManager(LPDIRECT3DDEVICE9 d3ddev, 
    std::shared_ptr<Shader> meshshader, 
    std::shared_ptr<Shader> groundshader) :
        m_selectedTool(NONE),
        m_d3ddev(d3ddev),
        m_selectedMesh(NO_INDEX),
        m_enableCreation(nullptr)
{
    m_templates.resize(MAX_OBJECT);
    m_meshes.resize(MAX_INSTANCES);

    std::generate(m_meshes.begin(), m_meshes.end(), 
        [&](){ return MeshManager::MeshPtr(new Mesh()); });

    std::generate(m_templates.begin(), m_templates.end(), 
        [&](){ return MeshManager::MeshPtr(new Mesh()); });

    // fill in the instance queue
    for(unsigned int i = 1; i < m_meshes.size(); ++i)
    {
        m_open.push(i);
    }

    auto createMesh = [&](const std::string& name, MeshPtr& mesh, std::shared_ptr<Shader> shader)
    {
        if(!mesh->Load(d3ddev, MODEL_FOLDER + name, shader))
        {
            Diagnostic::ShowMessage(name + " failed to load");
        }
    };

    // Create the sphere template
    createMesh("sphere.obj", m_templates[SPHERE], meshshader);
    m_templates[SPHERE]->SetVisible(false);
    m_templates[SPHERE]->CreateCollision(d3ddev, 1.5f, 8);

    // Create the box template
    createMesh("box.obj", m_templates[BOX], meshshader);
    m_templates[BOX]->SetVisible(false);
    m_templates[BOX]->CreateCollision(d3ddev, 1.9f, 1.9f, 1.9f);

    // Create the cylinder template
    createMesh("cylinder.obj", m_templates[CYLINDER], meshshader);
    m_templates[CYLINDER]->SetVisible(false);
    m_templates[CYLINDER]->CreateCollision(d3ddev, 2.2f, 3.4f, 8);

    // Create the ground plain
    createMesh("ground.obj", m_meshes[GROUND_INDEX], groundshader);
    m_meshes[GROUND_INDEX]->LoadTexture(d3ddev, TEXTURE_FOLDER+"ground.png");
    m_meshes[GROUND_INDEX]->SetPosition(0,-20,0);
    m_meshes[GROUND_INDEX]->SetPickable(false);
    m_meshes[GROUND_INDEX]->CreateCollision(d3ddev,150.0f,1.0f,150.0f);
}

void MeshManager::ChangeTool(Tool tool)
{
    m_selectedTool = tool;
}

void MeshManager::SetMeshEnableCallback(SetFlag enableCreation)
{
    m_enableCreation = enableCreation;
}

void MeshManager::AddObject(Object object)
{
    if(!m_open.empty())
    {
        unsigned int index = m_open.front();
        m_meshes[index]->SetVisible(true);
        m_meshes[index]->LoadAsInstance(m_d3ddev, m_templates[object]->GetData(), index);
        m_meshes[index]->SetColor(0.75f, 0.75f, 0.75f);
        m_meshes[index]->SetPosition(1.0f, 0.0f, 0.0f);

        m_meshes[index]->SetMeshPickFunction(std::bind(
            &MeshManager::SetSelectedMesh, this, m_meshes[index].get()));

        m_open.pop();

        if(m_open.empty())
        {
            m_enableCreation(false);
        }
    }
}

void MeshManager::RemoveScene()
{
    SetSelectedMesh(nullptr);
    for(unsigned int i = 1; i < m_meshes.size(); ++i)
    {
        if(m_meshes[i]->IsVisible())
        {
            m_meshes[i]->SetVisible(false);
            m_open.push(i);
        }
    }
    m_enableCreation(true);
}

void MeshManager::RemoveObject()
{
    if(m_selectedMesh != NO_INDEX)
    {
        if(m_open.empty())
        {
            m_enableCreation(true);
        }
        m_open.push(m_selectedMesh);
        SetSelectedMesh(nullptr);
    }
}

void MeshManager::SetSelectedMesh(const Mesh* mesh)
{
    if(m_selectedMesh != NO_INDEX)
    {
        m_meshes[m_selectedMesh]->SetSelected(false);
    }

    if(mesh && m_selectedMesh != mesh->GetIndex())
    {
        m_selectedMesh = mesh->GetIndex();
        m_meshes[m_selectedMesh]->SetSelected(true);
    }
    else
    {
        m_selectedMesh = NO_INDEX;
    }
}

void MeshManager::Draw(const D3DXVECTOR3& position, const Transform& projection, const Transform& view)
{
    if(Diagnostic::AllowText())
    {
        Diagnostic::Get().UpdateText("QueueFront", Diagnostic::WHITE,
            StringCast(m_open.empty() ? 0 : m_open.front()));

        Diagnostic::Get().UpdateText("QueueSize", Diagnostic::WHITE,
            StringCast(m_open.size()));

        Diagnostic::Get().UpdateText("SelectedMesh", Diagnostic::WHITE, 
            StringCast(m_selectedMesh));
    }

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