-- Retr01 Multi-World Layout Visualizer
-- Renders 7 world layouts on a 640x360 canvas, scaled 2x sharply.
-- Capped at a maximum world grid size of 64x64.

local worlds = {}
local maxScreens = 64
local canvas
local customFont

function love.load()
  love.window.setTitle("Retr01 Multi-World Visualizer")
  
  -- Force sharp pixel scaling for the canvas and fonts
  love.graphics.setDefaultFilter("nearest", "nearest")
  love.graphics.setLineStyle("rough")
  
  -- The physical window is 1280x720, but the internal canvas is 640x360 (2x scale)
  love.window.setMode(1280, 720, {resizable = false})
  canvas = love.graphics.newCanvas(640, 360)
  
  -- Safely load the Aseprite font if it exists in the directory
  local success = pcall(function()
    customFont = love.graphics.newFont("AsepriteFont.ttf", 16)
  end)
  if success and customFont then
    love.graphics.setFont(customFont)
  else
    love.graphics.setNewFont(16) -- Fallback
  end

  math.randomseed(os.time())
  generateAllWorlds()
end

function love.keypressed(key)
  if key == "escape" then
    love.event.quit()
  else
    generateAllWorlds()
  end
end

function love.mousepressed(x, y, button)
  generateAllWorlds()
end

function generateAllWorlds()
  worlds = {}
  table.insert(worlds, buildWorld(1, "1x1 Single"))
  table.insert(worlds, buildWorld(2, "Linear Horiz"))
  table.insert(worlds, buildWorld(3, "Linear Vert"))
  table.insert(worlds, buildWorld(4, "Snake Path"))
  table.insert(worlds, buildWorld(5, "Packed Grid"))
  table.insert(worlds, buildWorld(6, "Hole Grid"))
  table.insert(worlds, buildWorld(7, "Random Cluster"))
end

function buildWorld(type, name)
  local world = {name = name, cells = {}, bounds = {}}
  local startX, startY = 128, 128 
  
  if type == 1 then
    table.insert(world.cells, {x = startX, y = startY})

  elseif type == 2 then
    local length = love.math.random(8, 16)
    for i = 0, length - 1 do
      table.insert(world.cells, {x = startX - math.floor(length/2) + i, y = startY})
    end

  elseif type == 3 then
    local length = love.math.random(8, 16)
    for i = 0, length - 1 do
      table.insert(world.cells, {x = startX, y = startY - math.floor(length/2) + i})
    end

  elseif type == 4 then
    -- Snake Logic with strict 64x64 boundary enforcement
    local visited = {}
    local cx, cy = startX, startY
    table.insert(world.cells, {x = cx, y = cy})
    visited[cx .. "," .. cy] = true
    
    local dirs = {{1,0}, {-1,0}, {0,1}, {0,-1}}
    local currentDir = love.math.random(1, 4)
    local steps = 0
    local maxSteps = love.math.random(4, 10)
    
    while #world.cells < maxScreens do
      local dx, dy = dirs[currentDir][1], dirs[currentDir][2]
      local nx, ny = cx + dx, cy + dy
      
      -- Enforce 64x64 limit relative to the starting point
      if math.abs(nx - startX) < 32 and math.abs(ny - startY) < 32 then
        if not visited[nx .. "," .. ny] then
          cx, cy = nx, ny
          table.insert(world.cells, {x = cx, y = cy})
          visited[cx .. "," .. cy] = true
          steps = steps + 1
          
          if steps >= maxSteps then
            currentDir = (currentDir <= 2) and love.math.random(3, 4) or love.math.random(1, 2)
            steps = 0
            maxSteps = love.math.random(4, 10)
          end
        else
          local turnOpts = (currentDir <= 2) and {3, 4} or {1, 2}
          local freeOpts = {}
          for _, opt in ipairs(turnOpts) do
            local tx, ty = cx + dirs[opt][1], cy + dirs[opt][2]
            if math.abs(tx - startX) < 32 and math.abs(ty - startY) < 32 and not visited[tx .. "," .. ty] then
              table.insert(freeOpts, opt)
            end
          end
          
          if #freeOpts > 0 then
            currentDir = freeOpts[love.math.random(#freeOpts)]
            steps = 0
            maxSteps = love.math.random(4, 10)
          else
            break
          end
        end
      else
        -- Hit the 64x64 virtual border, force a turn inward
        currentDir = love.math.random(1, 4)
        steps = 0
      end
    end

  elseif type == 5 then
    for y = 0, 7 do
      for x = 0, 7 do
        table.insert(world.cells, {x = startX - 4 + x, y = startY - 4 + y})
      end
    end

  elseif type == 6 then
    for y = 0, 7 do
      for x = 0, 7 do
        if love.math.random() > love.math.random(30, 50) / 100 then
          table.insert(world.cells, {x = startX - 4 + x, y = startY - 4 + y})
        end
      end
    end

  elseif type == 7 then
    local visited = {}
    local cx, cy = startX, startY
    table.insert(world.cells, {x = cx, y = cy})
    visited[cx .. "," .. cy] = true
    
    while #world.cells < maxScreens do
      local base = world.cells[love.math.random(#world.cells)]
      local dx, dy = 0, 0
      local r = love.math.random(1, 4)
      if r == 1 then dx = 1 elseif r == 2 then dx = -1 elseif r == 3 then dy = 1 else dy = -1 end
      
      local nx, ny = base.x + dx, base.y + dy
      -- Enforce 64x64 bounds check
      if math.abs(nx - startX) < 32 and math.abs(ny - startY) < 32 then
        if not visited[nx .. "," .. ny] then
          visited[nx .. "," .. ny] = true
          table.insert(world.cells, {x = nx, y = ny})
        end
      end
    end
  end
  
  if #world.cells > 0 then
    world.bounds.minX, world.bounds.maxX = world.cells[1].x, world.cells[1].x
    world.bounds.minY, world.bounds.maxY = world.cells[1].y, world.cells[1].y
    for _, cell in ipairs(world.cells) do
      if cell.x < world.bounds.minX then world.bounds.minX = cell.x end
      if cell.x > world.bounds.maxX then world.bounds.maxX = cell.x end
      if cell.y < world.bounds.minY then world.bounds.minY = cell.y end
      if cell.y > world.bounds.maxY then world.bounds.maxY = cell.y end
    end
    
    -- Safety clamp to absolute maximum 64x64 grid footprint
    if (world.bounds.maxX - world.bounds.minX + 1) > 64 then
      world.bounds.maxX = world.bounds.minX + 63
    end
    if (world.bounds.maxY - world.bounds.minY + 1) > 64 then
      world.bounds.maxY = world.bounds.minY + 63
    end
  end
  
  return world
end

function love.draw()
  -- Render to our 640x360 internal canvas
  love.graphics.setCanvas(canvas)
  love.graphics.clear(0.05, 0.05, 0.05)
  
  local cols, rows = 4, 2
  local viewW = 640 / cols
  local viewH = 360 / rows
  
  -- Draw global instructions in the 8th (empty) slot
  love.graphics.setColor(1, 1, 1)
  love.graphics.printf("Click or press\nany key to\nrandomize.\n\nESC to quit.", viewW * 3 + 10, viewH * 1 + 30, viewW - 20, "center")

  -- Draw each world
  for i, world in ipairs(worlds) do
    local col = (i - 1) % cols
    local row = math.floor((i - 1) / cols)
    
    local vx = col * viewW
    local vy = row * viewH
    
    drawWorldViewport(world, vx, vy, viewW, viewH)
  end
  
  -- Reset canvas and draw it scaled 2x to the physical window
  love.graphics.setCanvas()
  love.graphics.setColor(1, 1, 1, 1)
  love.graphics.draw(canvas, 0, 0, 0, 2, 2)
end

function drawWorldViewport(world, vx, vy, vw, vh)
  -- Draw viewport border
  love.graphics.setColor(0.3, 0.3, 0.3)
  love.graphics.rectangle("line", vx + 2, vy + 2, vw - 4, vh - 4)
  
  if #world.cells == 0 then return end
  
  local gridW = world.bounds.maxX - world.bounds.minX + 1
  local gridH = world.bounds.maxY - world.bounds.minY + 1
  
  -- Title and Info text
  love.graphics.setColor(0.8, 0.8, 0.8)
  love.graphics.print(world.name, vx + 8, vy + 6)
  love.graphics.setColor(0.5, 0.8, 0.5)
  love.graphics.print("Grid: " .. gridW .. "x" .. gridH, vx + 8, vy + 22)
  love.graphics.setColor(0.8, 0.6, 0.2)
  love.graphics.print("Screens: " .. #world.cells, vx + 8, vy + 38)
  
  -- Calculate scaling to fit the grid inside the viewport
  local padding = 10
  local textHeaderHeight = 56
  
  local cellW = 4
  local cellH = 3
  
  while ((cellW + 1) * gridW <= (vw - padding * 2)) and ((cellH + 1) * gridH <= (vh - textHeaderHeight - padding)) do
    cellW = cellW + 1
    cellH = cellH + 1
  end
  
  local offsetX = vx + (vw - (gridW * cellW)) / 2
  local offsetY = vy + textHeaderHeight + (vh - textHeaderHeight - (gridH * cellH)) / 2
  
  -- 1. Draw the Virtual Bounding Box Grid (Empty Cells)
  love.graphics.setColor(0.2, 0.2, 0.2, 0.8)
  for gy = 0, gridH - 1 do
    for gx = 0, gridW - 1 do
      local dx = offsetX + (gx * cellW)
      local dy = offsetY + (gy * cellH)
      love.graphics.rectangle("line", dx, dy, cellW, cellH)
    end
  end

  -- 2. Draw the Active Screens
  for _, cell in ipairs(world.cells) do
    -- Only draw cells that fall within the bounded box
    if cell.x >= world.bounds.minX and cell.x <= world.bounds.maxX and
       cell.y >= world.bounds.minY and cell.y <= world.bounds.maxY then
      local drawX = offsetX + ((cell.x - world.bounds.minX) * cellW)
      local drawY = offsetY + ((cell.y - world.bounds.minY) * cellH)
      
      love.graphics.setColor(0.2, 0.6, 0.8, 0.6)
      love.graphics.rectangle("fill", drawX, drawY, cellW, cellH)
      
      love.graphics.setColor(0.4, 0.9, 1.0, 1.0)
      love.graphics.rectangle("line", drawX, drawY, cellW, cellH)
    end
  end
end

function love.keypressed(k)
  if k == "escape" then
    love.event.quit()
  end
end
