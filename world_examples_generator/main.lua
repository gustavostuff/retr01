-- Retr01 Multi-World Layout Visualizer
-- Renders 7 world layouts on a 640x360 canvas, scaled 2x sharply.

local Worlds = require "worlds"
local Render = require "render"

local worlds = {}
local canvas

local function regenerate()
  worlds = Worlds.generateAll()
end

function love.load()
  love.graphics.setDefaultFilter("nearest", "nearest")
  love.graphics.setLineStyle("rough")
  canvas = love.graphics.newCanvas(640, 360)

  local ok, font = pcall(love.graphics.newFont, "AsepriteFont.ttf", 16)
  if ok and font then
    love.graphics.setFont(font)
  else
    love.graphics.setNewFont(16)
  end

  math.randomseed(os.time())
  regenerate()
end

function love.keypressed(key)
  if key == "escape" then
    love.event.quit()
  else
    regenerate()
  end
end

function love.mousepressed()
  regenerate()
end

function love.draw()
  Render.draw(worlds, canvas)
end
