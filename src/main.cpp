#include <main.h>

int main(int argv, char ** argc)
{

    int durationSeconds = 10;

    if (argv >= 3)
    {
        std::map<std::string, std::string> args;
        std::vector<std::string> inputs;
        for (int i = 1; i < argv; i++)
        {
            inputs.push_back(argc[i]);
        }
        std::reverse(inputs.begin(), inputs.end());
        while (inputs.size() >= 2)
        {
            std::string arg = inputs.back();
            inputs.pop_back();
            args[arg] = inputs.back();
            inputs.pop_back();
        }

        if (args.find("-durationSeconds") != args.end())
        {
            durationSeconds = std::stoi(args["-durationSeconds"]);
        }
    }

    jGL::DesktopDisplay::Config conf;

    conf.VULKAN = false;

    #ifdef MACOS
    conf.COCOA_RETINA = true;
    #endif
    jGL::DesktopDisplay display(glm::ivec2(resX, resY), "GPGPU Sand", conf);
    display.setFrameLimit(60);

    glewInit();

    jGLInstance = std::move(std::make_unique<jGL::GL::OpenGLInstance>(glm::ivec2(resX,resY)));

    jGL::OrthoCam camera(resX, resY, glm::vec2(0.0,0.0));

    camera.setPosition(0.0f, 0.0f);

    jLog::Log log;

    high_resolution_clock::time_point tic, tock;
    double rdt = 0.0;

    jGLInstance->setTextProjection(glm::ortho(0.0,double(resX),0.0,double(resY)));
    jGLInstance->setMSAA(16);

    RNG rng;

    std::shared_ptr<jGL::Shader> shader = std::make_shared<jGL::GL::glShader>
    (
        jGL::GL::glShapeRenderer::shapeVertexShader,
        jGL::GL::glShapeRenderer::rectangleFragmentShader
    );

    shader->use();

    double delta = 0.0;
    double dt = 0.125e-3;
    jGL::ShapeRenderer::UpdateInfo uinfo;

    int n = cells*cells;
    int m = cells/2;

    std::vector<float> noise(m*m, 0.0);
    std::vector<float> spawnNoise(m*m, 0.0);
    std::vector<float> states(n, 0.0);
    std::vector<float> margolus(m*m, 0.0);
    std::vector<float> obstacles(n, 0.0);
    std::vector<float> density(n, 0.0);

    for (int i = 0; i < m*m; i++)
    {
        noise[i] = rng.nextFloat();
        spawnNoise[i] = rng.nextFloat();
    }

    for (int i = 0; i < n; i++)
    {
        //states[i] = rng.nextFloat()<0.1;
    }

    glCompute update
    (
        {
            {"cells", {cells, cells, 1}},
            {"noise", {m, m, 1}},
            {"margolus", {m, m, 1}},
            {"obstacles", {cells, cells, 1}}
        },
        {m, m, 1},
        1,
        blockCAComputeShader
    );

    glCompute toMargolus
    (
        {
            {"cells", {cells, cells, 1}},
            {"noise", {cells, cells, 1}}
        },
        {m, m, 1},
        1,
        toMargolusShader
    );

    glCompute fromMargolus
    (
        {
            {"margolus", {m, m, 1}}
        },
        {cells, cells, 1},
        1,
        fromMargolusShader
    );

    update.set("noise", noise);
    update.set("margolus", margolus);
    update.set("cells", states);
    update.set("obstacles", obstacles);
    update.sync();

    float pswap = 0.1;
    float pfriction = 0.1;
    float pslide = 0.9;
    update.shader.setUniform("p1", pswap);
    update.shader.setUniform("p2", pswap);
    update.shader.setUniform("p31", pfriction);
    update.shader.setUniform("p32", pswap);
    update.shader.setUniform("p6", pslide);
    update.shader.setUniform("p7", pfriction);
    update.shader.setUniform("p9", pslide);
    update.shader.setUniform("p11", pfriction);

    toMargolus.set("cells", states);
    toMargolus.set("noise", spawnNoise);
    toMargolus.sync();
    toMargolus.shader.setUniform("width", cells);
    toMargolus.shader.setUniform("type", 0);
    toMargolus.shader.setUniform("spawnProb", 0.0000001f);

    fromMargolus.set("margolus", margolus);
    fromMargolus.sync();
    fromMargolus.shader.setUniform("width", cells);
    fromMargolus.shader.setUniform("type", 0);

    float scale = cells/resX;

    Visualise vis(update.getTexture("cells"), update.getTexture("obstacles"));
    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_POINT_SPRITE);

    bool placeing = false; bool removing = false;
    bool reset = false;
    int type = 0;
    paused = true;

    auto start = std::chrono::steady_clock::now();

    while (display.isOpen())
    {
        tic = high_resolution_clock::now();

        if (display.keyHasEvent(GLFW_KEY_DOWN, jGL::EventType::PRESS))
        {
            camera.incrementZoom(-1.0f);
        }
        if (display.keyHasEvent(GLFW_KEY_UP, jGL::EventType::PRESS))
        {
            camera.incrementZoom(1.0f);
        }

        if (display.keyHasEvent(GLFW_KEY_SPACE, jGL::EventType::PRESS))
        {
            paused = !paused;
        }

        if (display.keyHasEvent(GLFW_KEY_R, jGL::EventType::PRESS))
        {
            reset = true;
        }

        if (display.keyHasEvent(GLFW_MOUSE_BUTTON_LEFT, jGL::EventType::PRESS) || display.keyHasEvent(GLFW_MOUSE_BUTTON_LEFT, jGL::EventType::HOLD))
        {
            placeing = true;
            removing = false;
        }
        else if (display.keyHasEvent(GLFW_MOUSE_BUTTON_LEFT, jGL::EventType::RELEASE))
        {
            placeing = false;
        }

        if (display.keyHasEvent(GLFW_MOUSE_BUTTON_RIGHT, jGL::EventType::PRESS) || display.keyHasEvent(GLFW_MOUSE_BUTTON_RIGHT, jGL::EventType::HOLD))
        {
            removing = true;
            placeing = false;
        }
        else if (display.keyHasEvent(GLFW_MOUSE_BUTTON_RIGHT, jGL::EventType::RELEASE))
        {
            removing = false;
        }

        if (placeing || removing)
        {
            double mouseX, mouseY;
            display.mousePosition(mouseX,mouseY);
            float value = 0.0;
            if (placeing) { value = 1.0; }
            if (removing) { value = 0.0; }
            placeOrRemove(obstacles, int(mouseX), int(resY-mouseY), 16, resX, value);
            update.set("obstacles", obstacles);
            update.sync("obstacles");
        }

        if (!paused)
        {
            if (reset) { update.shader.setUniform("reset", 1.0f); }

            toMargolus.shader.setUniform("type", type);
            toMargolus.shader.setUniform("seed", rng.nextFloat());
            fromMargolus.shader.setUniform("type", type);
            type = 1-type;

            toMargolus.glCopyTexture
            (
                update.getTexture("cells"),
                toMargolus.getTexture("cells"),
                {cells, cells}
            );
            toMargolus.compute(false);
            toMargolus.glCopyTexture
            (
                toMargolus.outputTexture(0),
                update.getTexture("margolus"),
                {m, m}
            );

            update.shader.setUniform("seed", glm::vec2(rng.nextFloat(), rng.nextFloat()));
            update.compute(false);
            update.glCopyTexture
            (
                update.outputTexture(0),
                fromMargolus.getTexture("margolus"),
                {m, m}
            );

            fromMargolus.compute(false);
            fromMargolus.glCopyTexture
            (
                fromMargolus.outputTexture(0),
                update.getTexture("cells"),
                {cells, cells}
            );

            if (reset) { update.shader.setUniform("reset", 0.0f); reset = false; }
        }
        glClearColor(0.0,0.0,0.0,1.0);
        glClear(GL_COLOR_BUFFER_BIT);
        vis.drawParticles(n, scale, camera.getVP());
        //vis.drawObstacles(obstacles.size(), scale, camera.getVP());

        delta = 0.0;
        for (int n = 0; n < 60; n++)
        {
            delta += deltas[n];
        }
        delta /= 60.0;

        if (frameId == 59)
        {
            std::cout << "FPS: " << fixedLengthNumber(1.0/delta,4) << "\n";
        }

        display.loop();

        tock = high_resolution_clock::now();

        deltas[frameId] = duration_cast<duration<double>>(tock-tic).count();
        frameId = (frameId+1) % 60;
        uinfo.scale = false;

    }

    jGLInstance->finish();

    return 0;
}