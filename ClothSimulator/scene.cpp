////////////////////////////////////////////////////////////////////////////////////////
// Kara Jensen - mail@karajensen.com - scene.cpp
////////////////////////////////////////////////////////////////////////////////////////

#include "scene.h"
#include "mesh.h"
#include "shader.h"
#include "collisionsolver.h"
#include "picking.h"
#include "manipulator.h"
#include "light.h"

namespace
{
    const D3DXVECTOR3 GROUND_POS(0.0f, -20.0f, 0.0f); ///< Starting position for ground
    const D3DXVECTOR3 INITIAL_POS(1.0f, 0.0f, 0.0f);  ///< Starting position for meshes

    const float MESH_COLOR = 0.75f; ///< Mesh rendering color;
    const int MAX_INSTANCES = 6;    ///< Maximum allowed objects including ground
    const int GROUND_INDEX = 0;     ///< Index for the ground mesh

    const std::string MODEL_FOLDER(".\\Resources\\Models\\");
    const std::string TEXTURE_FOLDER(".\\Resources\\Textures\\");
}

Scene::Scene(EnginePtr engine) :
    m_engine(engine),
    m_selectedMesh(NO_INDEX),
    m_enableCreation(nullptr)
{
    m_manipulator.reset(new Manipulator(engine));
    m_templates.resize(MAX_OBJECT);
    m_meshes.resize(MAX_INSTANCES);

    std::generate(m_meshes.begin(), m_meshes.end(), 
        [&](){ return Scene::MeshPtr(new Mesh(engine)); });

    std::generate(m_templates.begin(), m_templates.end(), 
        [&](){ return Scene::MeshPtr(new Mesh(engine)); });

    // fill in the instance queue
    for(unsigned int i = 1; i < m_meshes.size(); ++i)
    {
        m_open.push(i);
    }

    auto createMesh = [&](const std::string& name, 
        MeshPtr& mesh, ShaderManager::SceneShader shader)
    {
        auto meshshader = engine->getShader(shader);
        if(!mesh->Load(engine->device(), MODEL_FOLDER + name, meshshader))
        {
            ShowMessageBox(name + " failed to load");
        }
    };

    // Create the sphere template
    createMesh("sphere.obj", m_templates[SPHERE], ShaderManager::MAIN_SHADER);
    m_templates[SPHERE]->SetVisible(false);
    m_templates[SPHERE]->CreateCollision(engine->device(), 1.9f, 8);
    m_templates[SPHERE]->LoadTexture(engine->device(), TEXTURE_FOLDER+"sphere.png", 4, 1);

    // Create the box template
    createMesh("box.obj", m_templates[BOX], ShaderManager::MAIN_SHADER);
    m_templates[BOX]->SetVisible(false);
    m_templates[BOX]->CreateCollision(engine->device(), 3.7f, 3.7f, 3.7f);
    m_templates[BOX]->LoadTexture(engine->device(), TEXTURE_FOLDER+"box.png", 1024, 1);

    // Create the cylinder template
    createMesh("cylinder.obj", m_templates[CYLINDER], ShaderManager::MAIN_SHADER);
    m_templates[CYLINDER]->SetVisible(false);
    m_templates[CYLINDER]->CreateCollision(engine->device(), 2.2f, 3.4f, 8);
    m_templates[CYLINDER]->LoadTexture(engine->device(), TEXTURE_FOLDER+"cylinder.png", 1024, 1);

    // Create the ground plain
    createMesh("ground.obj", m_meshes[GROUND_INDEX], ShaderManager::GROUND_SHADER);
    m_meshes[GROUND_INDEX]->LoadTexture(engine->device(), TEXTURE_FOLDER+"ground.png", 512, 6);
    m_meshes[GROUND_INDEX]->SetPosition(GROUND_POS);
    m_meshes[GROUND_INDEX]->SetPickable(false);
    m_meshes[GROUND_INDEX]->CreateCollision(engine->device(),130.0f,1.0f,130.0f);
}

void Scene::LoadGuiCallbacks(GuiCallbacks* callbacks)
{
    m_enableCreation = callbacks->enableMeshCreation;

    callbacks->setMoveTool = std::bind(&Manipulator::ChangeTool,
        m_manipulator.get(), Manipulator::MOVE);

    callbacks->setRotateTool = std::bind(&Manipulator::ChangeTool,
        m_manipulator.get(), Manipulator::ROTATE);

    callbacks->setScaleTool = std::bind(&Manipulator::ChangeTool,
        m_manipulator.get(), Manipulator::SCALE);

    callbacks->setAnimateTool = std::bind(&Manipulator::ChangeTool,
        m_manipulator.get(), Manipulator::ANIMATE);
}

void Scene::AddObject(Object object)
{
    if(!m_open.empty())
    {
        unsigned int index = m_open.front();

        m_meshes[index]->SetVisible(true);
        m_meshes[index]->LoadAsInstance(m_engine->device(), 
            m_templates[object]->GetCollision(), 
            m_templates[object]->GetData(), index);

        m_meshes[index]->SetColor(MESH_COLOR, MESH_COLOR, MESH_COLOR);
        m_meshes[index]->SetPosition(INITIAL_POS);

        m_meshes[index]->SetMeshPickFunction(std::bind(
            &Scene::SetSelectedMesh, this, m_meshes[index].get()));

        m_open.pop();

        if(m_open.empty())
        {
            m_enableCreation(false);
        }
    }
}

void Scene::RemoveScene()
{
    SetSelectedMesh(nullptr);
    for(unsigned int i = 1; i < m_meshes.size(); ++i)
    {
        if(m_meshes[i]->IsVisible())
        {
            m_meshes[i]->SetVisible(false);
            m_meshes[i]->ResetAnimation();
            m_open.push(i);
        }
    }
    m_enableCreation(true);
}

void Scene::RemoveObject()
{
    if(m_selectedMesh != NO_INDEX)
    {
        if(m_open.empty())
        {
            m_enableCreation(true);
        }
        m_open.push(m_selectedMesh);
        m_meshes[m_selectedMesh]->SetVisible(false);
        m_meshes[m_selectedMesh]->ResetAnimation();
        SetSelectedMesh(nullptr);
    }
}

void Scene::SetSelectedMesh(const Mesh* mesh)
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

void Scene::DrawTools(const D3DXVECTOR3& position, 
    const Matrix& projection, const Matrix& view)
{
    if(m_selectedMesh != NO_INDEX)
    {
        m_manipulator->Render(projection, view,
            position, m_meshes[m_selectedMesh]);
    }
}

void Scene::Draw(const D3DXVECTOR3& position, 
    const Matrix& projection, const Matrix& view)
{
    std::for_each(m_meshes.begin(), m_meshes.end(), [&](MeshPtr& mesh)
    {
        if(mesh.get() && mesh->IsVisible())
        {
            mesh->DrawMesh(position, projection, view);
        }
    });
}

void Scene::DrawCollision(const Matrix& projection, const Matrix& view)
{
    std::for_each(m_meshes.begin(), m_meshes.end(), [&](MeshPtr& mesh)
    {
        if(mesh.get() && mesh->IsVisible())
        {
            mesh->DrawCollision(projection, view);
        }
    });
}

void Scene::ManipulatorPickingTest(Picking& picking)
{
    if(m_selectedMesh != NO_INDEX)
    {
        m_manipulator->MousePickTest(picking);
    }
}

void Scene::ScenePickingTest(Picking& picking)
{
    std::for_each(m_meshes.begin(), m_meshes.end(), [&](MeshPtr& mesh)
    {
        if(mesh.get() && mesh->IsVisible())
        {
            mesh->MousePickingTest(picking);
        }
    });
}

void Scene::UpdateState(bool pressed, const D3DXVECTOR2& direction,
        const Matrix& world, const Matrix& invProjection, float deltatime)
{
    if(m_engine->diagnostic()->AllowText())
    {
        m_engine->diagnostic()->UpdateText("QueueFront", 
            Diagnostic::WHITE, StringCast(m_open.empty() ? 0 : m_open.front()));

        m_engine->diagnostic()->UpdateText("QueueSize", 
            Diagnostic::WHITE, StringCast(m_open.size()));

        m_engine->diagnostic()->UpdateText("SelectedMesh", 
            Diagnostic::WHITE, StringCast(m_selectedMesh));
    }

    if(m_selectedMesh != NO_INDEX)
    {
        m_manipulator->UpdateState(m_meshes[m_selectedMesh],
            direction, world, invProjection, pressed, deltatime);
    }
}

void Scene::SolveClothCollision(CollisionSolver& solver)
{
    solver.SolveGroundCollision(*m_meshes[GROUND_INDEX]->GetCollision());

    for(unsigned int i = 1; i < m_meshes.size(); ++i)
    {
        if(m_meshes[i].get() && m_meshes[i]->IsVisible() && m_meshes[i]->HasCollision())
        {
            const Collision* collision = m_meshes[i]->GetCollision();
            switch(collision->GetShape())
            {
            case Collision::BOX:
                solver.SolveBoxCollision(*collision);
                break;
            case Collision::SPHERE:
                solver.SolveSphereCollision(*collision);
                break;
            case Collision::CYLINDER:
                solver.SolveCylinderCollision(*collision);
                break;
            }
        }
    }

    solver.SolveSelfCollision();
}

void Scene::SetCollisionVisibility(bool visible)
{
    std::for_each(m_meshes.begin(), m_meshes.end(), [&](MeshPtr& mesh)
    {
        if(mesh.get())
        {
            mesh->SetCollisionVisibility(visible);
        }
    });
}