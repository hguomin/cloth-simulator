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
#include "partition.h"
#include "collisionmesh.h"

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
    const int MAX_INSTANCES = 10;   ///< Maximum allowed objects
    const std::string MODEL_FOLDER(".\\Resources\\Models\\");
    const std::string TEXTURE_FOLDER(".\\Resources\\Textures\\");
}

Scene::Scene(EnginePtr engine, std::shared_ptr<CollisionSolver> solver) :
    m_engine(engine),
    m_selectedMesh(NO_INDEX),
    m_diagnosticMesh(NO_INDEX),
    m_enableCreation(nullptr),
    m_drawCollisions(false),
    m_drawWalls(false),
    m_wallMinBounds(0.0, 0.0, 0.0),
    m_wallMaxBounds(0.0, 0.0, 0.0),
    m_solver(solver)
{
    m_manipulator.reset(new Manipulator(engine));
    m_templates.resize(MAX_OBJECT);
    m_meshes.resize(MAX_INSTANCES);
    m_walls.resize(MAX_WALLS);
    m_ground.reset(new Mesh(engine));

    std::generate(m_walls.begin(), m_walls.end(), 
        [&engine](){ return Scene::CollisionPtr(new CollisionMesh(engine)); });

    std::generate(m_meshes.begin(), m_meshes.end(), 
        [&engine](){ return Scene::MeshPtr(new Mesh(engine)); });

    std::generate(m_templates.begin(), m_templates.end(), 
        [&engine](){ return Scene::MeshPtr(new Mesh(engine)); });

    // fill in the instance queue
    for(unsigned int i = 0; i < m_meshes.size(); ++i)
    {
        m_open.push(i);
    }

    auto createMesh = [&engine](const std::string& name, 
        MeshPtr& mesh, ShaderManager::SceneShader shader)
    {
        auto meshshader = engine->getShader(shader);
        mesh->LoadMesh(engine->device(), MODEL_FOLDER + name, meshshader);
    };

    // Create the sphere prototype
    D3DXVECTOR3 sMinScale(2.2f, 2.2f, 2.2f);
    D3DXVECTOR3 sMaxScale(2.0f, 2.0f, 2.0f);
    createMesh("sphere.obj", m_templates[SPHERE], ShaderManager::MAIN_SHADER);
    m_templates[SPHERE]->SetVisible(false);
    m_templates[SPHERE]->SetMaximumScale(4.0f, 4.0f, 4.0f);
    m_templates[SPHERE]->LoadTexture(TEXTURE_FOLDER+"sphere.png", 4, 1);
    m_templates[SPHERE]->InitialiseCollision(Geometry::SPHERE, sMinScale, sMaxScale, 10);

    // Create the box prototype
    D3DXVECTOR3 bMinScale(4.0f, 4.0f, 4.0f);
    D3DXVECTOR3 bMaxScale(3.41f, 3.41f, 3.41f);
    createMesh("box.obj", m_templates[BOX], ShaderManager::MAIN_SHADER);
    m_templates[BOX]->SetVisible(false);
    m_templates[BOX]->SetMaximumScale(4.0f, 4.0f, 4.0f);
    m_templates[BOX]->LoadTexture(TEXTURE_FOLDER+"box.png", 1024, 1);
    m_templates[BOX]->InitialiseCollision(Geometry::BOX, bMinScale, bMaxScale);

    // Create the cylinder prototype
    D3DXVECTOR3 cMinScale(2.25f, 2.25f, 3.5f);
    D3DXVECTOR3 cMaxScale(2.15f, 2.15f, 3.25f);
    createMesh("cylinder.obj", m_templates[CYLINDER], ShaderManager::MAIN_SHADER);
    m_templates[CYLINDER]->SetVisible(false);
    m_templates[CYLINDER]->SetMaximumScale(3.0f, 3.0f, 8.0f);
    m_templates[CYLINDER]->LoadTexture(TEXTURE_FOLDER+"cylinder.png", 1024, 1);
    m_templates[CYLINDER]->InitialiseCollision(Geometry::CYLINDER, cMinScale, cMaxScale, 10);

    // Create the ground grid
    const float groundHeight = 20.0f;
    createMesh("ground.obj", m_ground, ShaderManager::GROUND_SHADER);
    m_ground->LoadTexture(TEXTURE_FOLDER+"ground.png", 512, 6);
    m_ground->SetPosition(D3DXVECTOR3(0.0f, -groundHeight, 0.0f));

    // Create wall collision models
    const float wallSize = 130.0f;
    const float wallDepth = 0.1f;
    const float wallOffset = wallSize/2.0f;
    const float wallHeight = groundHeight * 2.0f;

    m_walls[FLOOR]->Initialise(true, Geometry::BOX, 
        D3DXVECTOR3(wallSize, wallDepth, wallSize));
    m_walls[ROOF]->Initialise(true, Geometry::BOX, 
        D3DXVECTOR3(wallSize, wallDepth, wallSize));
    m_walls[LEFT]->Initialise(true, Geometry::BOX, 
        D3DXVECTOR3(wallDepth, wallHeight, wallSize));
    m_walls[RIGHT]->Initialise(true, Geometry::BOX, 
        D3DXVECTOR3(wallDepth, wallHeight, wallSize));
    m_walls[FORWARD]->Initialise(true, Geometry::BOX, 
        D3DXVECTOR3(wallSize, wallHeight, wallDepth));
    m_walls[BACKWARD]->Initialise(true, Geometry::BOX,
        D3DXVECTOR3(wallSize, wallHeight, wallDepth));

    m_walls[FLOOR]->SetPosition(D3DXVECTOR3(0.0f, -groundHeight, 0.0f));
    m_walls[ROOF]->SetPosition(D3DXVECTOR3(0.0f, groundHeight, 0.0f));
    m_walls[LEFT]->SetPosition(D3DXVECTOR3(wallOffset, 0.0f, 0.0f));
    m_walls[RIGHT]->SetPosition(D3DXVECTOR3(-wallOffset, 0.0f, 0.0f));
    m_walls[FORWARD]->SetPosition(D3DXVECTOR3(0.0f, 0.0f, wallOffset));
    m_walls[BACKWARD]->SetPosition(D3DXVECTOR3(0.0f, 0.0f, -wallOffset));

    m_wallMaxBounds.x = wallOffset - wallDepth;
    m_wallMaxBounds.y = -groundHeight + wallDepth;
    m_wallMaxBounds.z = wallOffset - wallDepth;

    m_wallMinBounds.x = -wallOffset + wallDepth;
    m_wallMinBounds.y = groundHeight - wallDepth;
    m_wallMinBounds.z = -wallOffset + wallDepth;

    std::for_each(m_walls.begin(), m_walls.end(), [](const CollisionPtr& mesh)
    {
        mesh->SetColor(D3DXVECTOR3(0.5f, 0.0f, 0.5f));
        mesh->SetDraw(true);
    });

    std::for_each(m_meshes.begin(), m_meshes.end(), [](const MeshPtr& mesh)
    {
        mesh->SetVisible(false);
    });
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

Scene::~Scene()
{
}

void Scene::AddObject(Object object)
{
    if(!m_open.empty())
    {
        unsigned int index = m_open.front();

        m_meshes[index]->LoadAsInstance(m_engine->device(), 
            *m_templates[object], index);

        m_meshes[index]->SetVisible(true);
        m_meshes[index]->ResetAnimation();
        m_meshes[index]->SetColor(MESH_COLOR, MESH_COLOR, MESH_COLOR);
        m_meshes[index]->SetCollisionVisibility(true);

        m_meshes[index]->SetMeshPickFunction(std::bind(
            &Scene::SetSelectedMesh, this, m_meshes[index].get()));

        m_engine->octree()->AddObject(m_meshes[index]->GetCollisionMesh());

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
    for(const MeshPtr& mesh : m_meshes)
    {
        if(mesh->IsVisible())
        {
            RemoveMesh(mesh);
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

        RemoveMesh(m_meshes[m_selectedMesh]);
        SetSelectedMesh(nullptr);
    }
}

void Scene::RemoveMesh(const MeshPtr& mesh)
{
    mesh->SetVisible(false);
    mesh->ResetAnimation();
    m_open.push(mesh->GetIndex());
    m_engine->octree()->RemoveObject(mesh->GetCollisionMesh());
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
        m_diagnosticMesh = m_selectedMesh;
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
    auto drawMesh = [&position, &projection, &view](const MeshPtr& mesh)
    {
        if(mesh->IsVisible())
        {
            mesh->DrawMesh(position, projection, view);
            mesh->DrawDiagnostics();
        }
    };

    drawMesh(m_ground);
    std::for_each(m_meshes.begin(), m_meshes.end(), drawMesh);
}

void Scene::DrawCollisions(const Matrix& projection, const Matrix& view)
{
    if(m_drawWalls)
    {
        for(auto& wall : m_walls)
        {
            wall->DrawMesh(projection, view);
        }
    }

    if(m_drawCollisions)
    {
        for(auto& mesh : m_meshes)
        {
            if(mesh->IsVisible())
            {
                mesh->DrawCollisionMesh(projection, view);
            }
        }
    }
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
    std::for_each(m_meshes.begin(), m_meshes.end(), [&picking](const MeshPtr& mesh)
    {
        if(mesh->IsVisible())
        {
            mesh->MousePickingTest(picking);
        }
    });
}

void Scene::PreCollisionUpdate(bool pressed, const D3DXVECTOR2& direction,
        const Matrix& world, const Matrix& invProjection, float deltatime)
{
    if(m_engine->diagnostic()->AllowDiagnostics(Diagnostic::MESH))
    {
        m_engine->diagnostic()->UpdateText(Diagnostic::MESH, "QueueFront", 
            Diagnostic::WHITE, StringCast(m_open.empty() ? 0 : m_open.front()));

        m_engine->diagnostic()->UpdateText(Diagnostic::MESH, "QueueSize", 
            Diagnostic::WHITE, StringCast(m_open.size()));

        m_engine->diagnostic()->UpdateText(Diagnostic::MESH, "SelectedMesh", 
            Diagnostic::WHITE, StringCast(m_selectedMesh));

        if(m_diagnosticMesh != NO_INDEX)
        {
            const auto& mesh = m_meshes[m_diagnosticMesh];
            if(mesh->IsVisible())
            {
                const auto& collision = m_meshes[m_diagnosticMesh]->GetCollisionMesh();
                const Partition* partition = collision.GetPartition();
                const D3DXVECTOR3& velocity = collision.GetVelocity();
                const D3DXVECTOR3 scale = collision.GetLocalScale();

                m_manipulator->UpdateDiagnostics(m_meshes[m_diagnosticMesh]);

                m_engine->diagnostic()->UpdateText(Diagnostic::MESH, "PartitionID", 
                    Diagnostic::WHITE, partition ? partition->GetID() : "None", true);

                m_engine->diagnostic()->UpdateText(Diagnostic::MESH, "Velocity", 
                    Diagnostic::WHITE, StringCast(velocity.x) + " " +
                    StringCast(velocity.y) + " " + StringCast(velocity.z), true);

                m_engine->diagnostic()->UpdateText(Diagnostic::MESH, "LocalScale", 
                    Diagnostic::WHITE, StringCast(scale.x) + " " +
                    StringCast(scale.y) + " " + StringCast(scale.z), true);
            }
        }
    }

    if(m_selectedMesh != NO_INDEX)
    {
        m_manipulator->UpdateState(m_meshes[m_selectedMesh],
            direction, world, invProjection, pressed, deltatime);
    }

    std::for_each(m_meshes.begin(), m_meshes.end(), [deltatime](const MeshPtr& mesh)
    {
        if(mesh->IsVisible())
        {
            mesh->Animate(deltatime);
        }
    });
}

void Scene::PostCollisionUpdate()
{
    std::for_each(m_meshes.begin(), m_meshes.end(), [](const MeshPtr& mesh)
    {
        if(mesh->IsVisible())
        {
            mesh->UpdateCollision();
        }
    });
}

void Scene::SolveCollisions()
{
    m_solver->SolveClothCollision(m_wallMinBounds, m_wallMaxBounds);

    std::for_each(m_meshes.begin(), m_meshes.end(), [this](const MeshPtr& mesh)
    {
        if(mesh->IsVisible() && mesh->HasCollisionMesh())
        {
            m_engine->octree()->IterateOctree(mesh->GetCollisionMesh());
        }
    });
}

void Scene::SetCollisionVisibility(bool visible)
{
    m_drawCollisions = visible;
}

void Scene::ToggleWallVisibility()
{
    m_drawWalls = !m_drawWalls;
}