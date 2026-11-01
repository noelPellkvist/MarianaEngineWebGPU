--!strict
-- Snake: the classic game in 3D, the finished script of the "Your first game" tutorial.
-- Put it on an empty entity in a scene with a camera. Arrow keys or W A S D steer; eat the red
-- apples to grow; running into a wall or yourself ends the game, and the snake flies apart.
local Snake = {} :: Script

Snake.properties = {
    size = 15,          -- the board is size x size cells of 1 m
    stepTime = 0.16,    -- seconds per step at the start
    speedUp = 0.96,     -- each apple makes a step take this much of the time before
    startLength = 3,    -- segments at the start
}

local HEAD_COLOR = vec3(0.35, 0.95, 0.35)
local BODY_COLOR = vec3(0.15, 0.6, 0.2)
local APPLE_COLOR = vec3(0.9, 0.12, 0.1)
local WALL_COLOR = vec3(0.25, 0.27, 0.32)
local FLOOR_COLOR = vec3(0.75, 0.78, 0.72)

-- The arrow keys and W A S D as directions on the board (+Z is "up" on the screen).
local KEYS: { [string]: vector } = {
    up = vec3(0, 0, 1), w = vec3(0, 0, 1),
    down = vec3(0, 0, -1), s = vec3(0, 0, -1),
    left = vec3(-1, 0, 0), a = vec3(-1, 0, 0),
    right = vec3(1, 0, 0), d = vec3(1, 0, 0),
}

-- Where a cell (whole numbers x and z) is in the world: the board is centred on the entity.
local function cellToWorld(self: Script, cell: vector): vector
    local half = (self.size - 1) / 2
    return self.entity.worldPosition + vec3(cell.x - half, 0.45, cell.z - half)
end

local function sameCell(a: vector, b: vector): boolean
    return a.x == b.x and a.z == b.z
end

local function isInside(self: Script, cell: vector): boolean
    return cell.x >= 0 and cell.z >= 0 and cell.x < self.size and cell.z < self.size
end

-- The floor and the four walls, made once.
local function buildBoard(self: Script)
    local center = self.entity.worldPosition
    local floor = scene.SpawnPrimitive("plane", center, FLOOR_COLOR)
    floor.name = "Floor"
    floor.scale = vec3(self.size / 10, 1, self.size / 10) -- the plane is 10 m across
    -- An invisible slab under the floor, so what flies apart at the end lands on something.
    local ground = scene.Create("Ground", self.entity)
    ground.worldPosition = center - vec3(0, 0.5, 0)
    ground:Add("BoxCollider", { size = vec3(self.size + 2, 1, self.size + 2) })

    local half = self.size / 2 + 0.5
    local walls = {
        { vec3(0, 0.5, half), vec3(self.size + 2, 1, 1) },
        { vec3(0, 0.5, -half), vec3(self.size + 2, 1, 1) },
        { vec3(half, 0.5, 0), vec3(1, 1, self.size) },
        { vec3(-half, 0.5, 0), vec3(1, 1, self.size) },
    }
    for _, wall in walls do
        local block = scene.SpawnPrimitive("cube", center + wall[1], WALL_COLOR)
        block.name = "Wall"
        block.scale = wall[2]
        block:Add("BoxCollider") -- 1 m, scaled with the entity
    end
end

-- A free cell for the apple (not under the snake).
local function freeCell(self: Script): vector
    while true do
        local cell = vec3(math.random(0, self.size - 1), 0, math.random(0, self.size - 1))
        local taken = false
        for _, c in self.cells do
            if sameCell(c, cell) then
                taken = true
                break
            end
        end
        if not taken then
            return cell
        end
    end
end

local function placeApple(self: Script)
    self.appleCell = freeCell(self)
    if not (self.apple and self.apple:IsValid()) then
        self.apple = scene.SpawnPrimitive("sphere", nil, APPLE_COLOR)
        self.apple.name = "Apple"
        self.apple.scale = vec3(0.7, 0.7, 0.7)
    end
    self.apple.position = cellToWorld(self, self.appleCell)
end

local function addSegment(self: Script, cell: vector, color: vector): Entity
    local segment = scene.SpawnPrimitive("cube", cellToWorld(self, cell), color)
    segment.name = "Segment"
    segment.scale = vec3(0.85, 0.85, 0.85)
    table.insert(self.segments, segment)
    return segment
end

-- A new game: the snake in the middle, heading right, and an apple.
local function newGame(self: Script)
    self.cells = {}
    self.segments = {}
    self.from = {}
    local middle = math.floor(self.size / 2)
    for i = 0, self.startLength - 1 do
        local cell = vec3(middle - i, 0, middle)
        table.insert(self.cells, cell)
        table.insert(self.from, cell)
        addSegment(self, cell, if i == 0 then HEAD_COLOR else BODY_COLOR)
    end
    -- Eyes on the head, children of it, so they turn with it.
    local head = self.segments[1]
    for _, side in { -0.22, 0.22 } do
        local eye = scene.SpawnPrimitive("sphere", nil, vec3(0.05, 0.05, 0.05))
        eye.name = "Eye"
        eye.parent = head
        eye.position = vec3(side, 0.25, 0.45)
        eye.scale = vec3(0.22, 0.22, 0.22)
    end
    self.direction = vec3(1, 0, 0)
    self.queued = {}
    self.timer = 0
    self.score = 0
    self.currentStep = self.stepTime
    self.over = false
    placeApple(self)
end

-- The game is lost: every segment becomes a physics body and flies apart.
local function gameOver(self: Script)
    self.over = true
    self.overTime = 0
    self.best = math.max(self.best or 0, self.score)
    print(`Game over! Score {self.score}, best {self.best}`)
    for _, segment in self.segments do
        local away = vec3(math.random() - 0.5, 0, math.random() - 0.5) * 8
        segment:Add("BoxCollider")
        segment:Add("RigidBody", {
            motion = "Dynamic",
            mass = 1,
            startVelocity = away + vec3(0, 6 + math.random() * 4, 0),
            startAngularVelocity = vec3(math.random() * 10, math.random() * 10, math.random() * 10),
        })
    end
end

-- One step: the head moves on a cell, the rest follow; an apple makes the snake longer.
local function step(self: Script)
    if #self.queued > 0 then
        self.direction = table.remove(self.queued, 1) :: vector
    end
    local head = self.cells[1] + self.direction
    local eats = sameCell(head, self.appleCell)

    -- Walls and its own body (the tail moves away this step, unless the snake grows).
    local last = if eats then #self.cells else #self.cells - 1
    if not isInside(self, head) then
        gameOver(self)
        return
    end
    for i = 1, last do
        if sameCell(self.cells[i], head) then
            gameOver(self)
            return
        end
    end

    -- Every segment glides from the cell it is on to the one in front of it (the head to the new
    -- cell); `from` keeps where each one starts.
    self.from = table.clone(self.cells)
    table.insert(self.cells, 1, head)
    if eats then
        -- A new segment appears where the tail is and stays there for this step.
        local tail = self.from[#self.from]
        table.insert(self.from, tail)
        addSegment(self, tail, BODY_COLOR)
        self.score += 1
        self.currentStep *= self.speedUp
        print(`Score {self.score}`)
        placeApple(self)
    else
        table.remove(self.cells)
    end
    self.segments[1].rotation = quat.lookRotation(self.direction)
end

function Snake.Start(self: Script)
    math.randomseed(os.clock() * 1000)
    buildBoard(self)
    newGame(self)
end

function Snake.Update(self: Script, dt: number)
    -- The apple bobs and turns wherever the game is.
    if self.apple and self.apple:IsValid() then
        self.apple.position = cellToWorld(self, self.appleCell) + vec3(0, math.sin(time.time * 4) * 0.1, 0)
        self.apple:Rotate(vec3(0, 1, 0), 90 * dt)
    end

    if self.over then
        -- Two and a half seconds of flying pieces, then a new game.
        self.overTime += dt
        if self.overTime > 2.5 then
            for _, segment in self.segments do
                segment:Destroy()
            end
            newGame(self)
        end
        return
    end

    -- Turns are queued (two at most), so quick key presses all count; reversing isn't allowed.
    for key, direction in KEYS do
        if input.GetKeyDown(key) and #self.queued < 2 then
            local heading = if #self.queued > 0 then self.queued[#self.queued] else self.direction
            if not sameCell(direction, heading) and not sameCell(direction, -heading) then
                table.insert(self.queued, direction)
            end
        end
    end

    self.timer += dt
    while self.timer >= self.currentStep and not self.over do
        self.timer -= self.currentStep
        step(self)
    end
    if self.over then
        return
    end

    -- Between steps every segment glides from its old cell to its new one.
    local t = math.clamp(self.timer / self.currentStep, 0, 1)
    for i, segment in self.segments do
        local from = cellToWorld(self, self.from[i])
        local to = cellToWorld(self, self.cells[i])
        segment.position = vector.lerp(from, to, t)
    end
end

return Snake
