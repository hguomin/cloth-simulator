////////////////////////////////////////////////////////////////////////////////////////
// Kara Jensen - mail@karajensen.com - scene.cpp
////////////////////////////////////////////////////////////////////////////////////////

#include "scene.h"
#include "mesh.h"
#include "shader.h"
#include "collisionsolver.h"
#include "picking.h"
#include "manipulator.h"
#include "octree.h"
#include "light.h"

namespace
{
    /**
    * Walls that the cloth can collide with
    */
    enum Wall
    {
        FLOOR,
        ROOF,
        LEFT,
        RIGHT,
        FORWARD,
        BACKWARD,
        MAX_WALLS
    };

    const float MESH_COLOR = 0.75f; ///< Mesh rendering color
    const int MAX_INSTANCES = 9;    ///< Maximum allowed objects
    const std::string MODEL_FOLDER(".\\Resources\\Models\\");
    const std::string TEXTURE_FOLDER(".\\Resources\\Textures\\");
}

Scene::Scene(EnginePtr engine) :
    m_engine(engine),
    m_selectedMesh(NO_INDEX),
    m_enableCreation(nullptr),
    m_drawCollisions(false),
    m_octree(new Octree(engine))
{
    m_manipulator.reset(new Manipulator(engine));
    m_templates.resize(MAX_OBJECT);
    m_meshes.resize(MAX_INSTANCES);
    m_walls.resize(MAX_WALLS);
    m_ground.reset(new Mesh(engine));

    std::generate(m_walls.begin(), m_walls.end(), 
        [&](){ return Scene::MeshPtr(new Mesh(engine)); });

    std::generate(m_meshes.begin(), m_meshes.end(), 
        [&](){ return Scene::MeshPtr(new Mesh(engine)); });

    std::generate(m_templates.begin(), m_templates.end(), 
        [&](){ return Scene::MeshPtr(new Mesh(engine)); });

    // fill in the instance queue
    for(unsigned int i = 0; i < m_meshes.size(); ++i)
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
    m_templates[SPHERE]->CreateCollisionMesh(engine->device(), 2.2f, 8);
    m_templates[SPHERE]->LoadTexture(engine->device(), TEXTURE_FOLDER+"sphere.png", 4, 1);

    // Create the box template
    createMesh("box.obj", m_templates[BOX], ShaderManager::MAIN_SHADER);
    m_templates[BOX]->SetVisible(false);
    m_templates[BOX]->CreateCollisionMesh(engine->device(), 3.6f, 3.6f, 3.6f);
    m_templates[BOX]->LoadTexture(engine->device(), TEXTURE_FOLDER+"box.png", 1024, 1);

    // Create the cylinder template
    createMesh("cylinder.obj", m_templates[CYLINDER], ShaderManager::MAIN_SHADER);
    m_templates[CYLINDER]->SetVisible(false);
    m_templates[CYLINDER]->CreateCollisionMesh(engine->device(), 2.2f, 3.3f, 8);
    m_templates[CYLINDER]->LoadTexture(engine->device(), TEXTURE_FOLDER+"cylinder.png", 1024, 1);

    // Create the ground grid
    const float groundHeight = 20.0f;
    createMesh("ground.obj", m_ground, ShaderManager::GROUND_SHADER);
    m_ground->LoadTexture(engine->device(), TEXTURE_FOLDER+"ground.png", 512, 6);
    m_ground->SetPosition(D3DXVECTOR3(0.0f, -groundHeight, 0.0f));

    // Create wall collision models
    const float wallSize = 130.0f;
    const float wallDepth = 1.0f;
    const float wallOffset = wallSize/2.0f;
    const float wallHeight = groundHeight * 2.0f;

    m_walls[FLOOR]->CreateCollisionMesh(engine->device(), wallSize, wallDepth, wallSize);
    m_walls[ROOF]->CreateCollisionMesh(engine->device(), wallSize, wallDepth, wallSize);
    m_walls[LEFT]->CreateCollisionMesh(engine->device(), wallDepth, wallHeight, wallSize);
    m_walls[RIGHT]->CreateCollisionMesh(engine->device(), wallDepth, wallHeight, wallSize);
    m_walls[FORWARD]->CreateCollisionMesh(engine->device(), wallSize, wallHeight, wallDepth);
    m_walls[BACKWARD]->CreateCollisionMesh(engine->device(), wallSize, wallHeight, wallDepth);

    m_walls[FLOOR]->SetPosition(D3DXVECTOR3(0.0f, -groundHeight, 0.0f));
    m_walls[ROOF]->SetPosition(D3DXVECTOR3(0.0f, groundHeight, 0.0f));
    m_walls[LEFT]->SetPosition(D3DXVECTOR3(wallOffset, 0.0f, 0.0f));
    m_walls[RIGHT]->SetPosition(D3DXVECTOR3(-wallOffset, 0.0f, 0.0f));
    m_walls[FORWARD]->SetPosition(D3DXVECTOR3(0.0f, 0.0f, wallOffset));
    m_walls[BACKWARD]->SetPosition(D3DXVECTOR3(0.0f, 0.0f, -wallOffset));

    std::for_each(m_walls.begin(), m_walls.end(), [&](const MeshPtr& mesh)
    {
        mesh->GetCollisionMesh()->SetColor(D3DXVECTOR3(0.5f, 0.0f, 0.5f));
        mesh->SetCollisionVisibility(true);
    });

    // Create the octree partitioning
    m_octree->BuildInitialTree();
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
        const D3DXVECTOR3 position(1.0f, 0.0f, 0.0f); 

        m_meshes[index]->SetVisible(true);
        m_meshes[index]->LoadAsInstance(m_engine->device(), 
            m_templates[object]->GetCollisionMesh(),
            m_templates[object]->GetData(), index);

        m_meshes[index]->ResetAnimation();
        m_meshes[index]->SetColor(MESH_COLOR, MESH_COLOR, MESH_COLOR);
        m_meshes[index]->SetPosition(position);
        m_meshes[index]->SetCollisionVisibility(true);

        m_meshes[index]->SetMeshPickFunction(std::bind(
            &Scene::SetSelectedMesh, this, m_meshes[index].get()));

        m_octree->AddObject(m_meshes[index]->GetCollisionPtr());
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
    std::for_each(m_meshes.begin(), m_meshes.end(), [&](const MeshPtr& mesh)
    {
        if(mesh->IsVisible())
        {
            mesh->SetVisible(false);
            mesh->ResetAnimation();
            m_open.push(mesh->GetIndex());
        }
    });
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

void Scene::Draw(float deltatime, const D3DXVECTOR3& position, 
    const Matrix& projection, const Matrix& view)
{
    auto drawMesh = [&](const MeshPtr& mesh)
    {
        if(mesh.get() && mesh->IsVisible())
        {
            mesh->DrawMesh(position, projection, view, deltatime);
        }
    };

    drawMesh(m_ground);
    std::for_each(m_meshes.begin(), m_meshes.end(), drawMesh);
}

void Scene::DrawCollisions(const Matrix& projection, const Matrix& view)
{
    if(m_drawCollisions)
    {
        for(auto& wall : m_walls)
        {
            wall->DrawCollisionMesh(projection, view, false);
        }

        for(auto& mesh : m_meshes)
        {
            if(mesh.get() && mesh->IsVisible())
            {
                mesh->DrawCollisionMesh(projection, view);
            }
        }
    }

    m_octree->RenderDiagnostics();
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
    std::for_each(m_meshes.begin(), m_meshes.end(), [&](const MeshPtr& mesh)
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
    if(m_engine->diagnostic()->AllowDiagnostics(Diagnostic::GENERAL))
    {
        m_engine->diagnostic()->UpdateText(Diagnostic::GENERAL, "QueueFront", 
            Diagnostic::WHITE, StringCast(m_open.empty() ? 0 : m_open.front()));

        m_engine->diagnostic()->UpdateText(Diagnostic::GENERAL, "QueueSize", 
            Diagnostic::WHITE, StringCast(m_open.size()));

        m_engine->diagnostic()->UpdateText(Diagnostic::GENERAL, "SelectedMesh", 
            Diagnostic::WHITE, StringCast(m_selectedMesh));
    }

    if(m_selectedMesh != NO_INDEX)
    {
        m_manipulator->UpdateState(m_meshes[m_selectedMesh],
            direction, world, invProjection, pressed, deltatime);
    }
}

void Scene::SolveClothCollisions(CollisionSolver& solver)
{
    solver.SolveGroundCollisionMesh(*m_walls[FLOOR]->GetCollisionMesh());

    std::for_each(m_meshes.begin(), m_meshes.end(), [&](const MeshPtr& mesh)
    {
        if(mesh.get() && mesh->IsVisible() && mesh->HasCollisionMesh())
        {
            const CollisionMesh* collisionmesh = mesh->GetCollisionMesh();
            switch(collisionmesh->GetShape())
            {
            case CollisionMesh::BOX:
                solver.SolveBoxCollisionMesh(*collisionmesh);
                break;
            case CollisionMesh::SPHERE:
                solver.SolveSphereCollisionMesh(*collisionmesh);
                break;
            case CollisionMesh::CYLINDER:
                solver.SolveCylinderCollisionMesh(*collisionmesh);
                break;
            }
        }
    });

    solver.SolveSelfCollision();
}

void Scene::SetCollisionVisibility(bool visible)
{
    m_drawCollisions = visible;
}