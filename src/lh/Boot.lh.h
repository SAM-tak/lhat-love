/**
 * Copyright (c) 2006-2026 LOVE Development Team
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 **/

#ifndef LOVE_LH_BOOT_LH_H
#define LOVE_LH_BOOT_LH_H

// L^ units the engine embeds and serves through the loader's hold table.
// Raw string literals, the way boot.lua used to be included.

namespace love
{
namespace lh
{

// Boot.lh: the default `run`, the successor of callbacks.lua's love.run. A
// script whose return^ is the run procedure; yieldable, so calling it from
// C answers a coroutine the engine resumes once per frame. The handlers
// table holds one member per callback -- the game's own or a no-op --
// built by the engine (Boot.cpp), so every call here is statically typed.
static const char boot_lh[] = R"lh(import^love.boot
import^love.event
import^love.timer
import^love.graphics

return^p^{
    let^h = love.boot.handlers()
    h.load()

    # We don't want the first frame's dt to include time taken by load.
    love.timer.step()

    repeat^{
        love.event.pump()
        let^code = love.event.dispatch(h)
        if^code? { return^code }

        let^dt = love.timer.step()
        h.update(dt)

        if^love.graphics.isActive() {
            love.graphics.origin()
            love.graphics.clear()
            h.draw()
            love.graphics.present()
        }

        love.timer.sleep(0.001)
        yield^
    }
}
)lh";

// What runs when no game is given: nogame.lua's animated screen, drawn
// with what the L^ bindings offer rather than with its embedded artwork --
// a chain of letters swinging from a balloon (love.physics), clouds
// drifting past, and a drop that restarts into the game dropped on it.
static const char nogame_lh[] = R"lh(module^nogame

import^love.boot
import^love.graphics
import^love.physics
import^love.window
import^love.event
import^love.timer
import^love.keyboard
import^std.math

let^letters = { "n", "o", "g", "a", "m", "e" }

let^world = love.physics.newWorld(0, 700, true^)
var^anchor : love.physics.Body|nil^= nil^
var^balloon : love.physics.Body|nil^= nil^
var^segments : t^{love.physics.Body[]} = {}
var^clouds : t^{t^{ x : number^, y : number^, w : number^, speed : number^}[]} = {}
var^time = 0
var^blink = 0

# The chain hangs from a fixed point and holds the balloon at its end; the
# balloon pulls upwards, so the whole thing swings like nogame.lua's does.
let^build = p^{
    for^body in^world.getBodies() {
        if^!body.isDestroyed() { body.destroy() }
    }
    segments := {}

    let^w = love.graphics.getWidth()
    let^h = love.graphics.getHeight()
    let^top = h * 0.28
    let^length = 42
    anchor := love.physics.newBody(world, w / 2, top, love.physics.BodyType.static)

    var^previous = anchor
    for^i from^1 to^letters.length^{
        let^y = top + i * length
        let^body, shape = love.physics.newCircleBody(world, love.physics.BodyType.dynamic, w / 2, y, 17)
        shape.setDensity(1.2)
        shape.setFriction(0.4)
        body.resetMassData()
        body.setLinearDamping(0.4)
        body.setAngularDamping(0.5)
        if^previous fits^love.physics.Body {
            love.physics.newRevoluteJoint(previous, body, w / 2, y - length / 2, false^)
        }
        segments.push^(body)
        previous := body
    }

    # The last segment. Not segments[-1]: a negative ordinal counts from the
    # end for the builtins that take one (at, substr, slice^, remove^,
    # insert^), but a subscript is a key, and -1 is none.
    let^tail = segments[segments.length^ - 1]
    if^tail fits^love.physics.Body {
        let^tx, ty = tail.getPosition()
        let^ball, ballShape = love.physics.newCircleBody(world, love.physics.BodyType.dynamic, tx, ty + length * 1.8, 32)
        ballShape.setDensity(0.2)
        ballShape.setRestitution(0.4)
        ball.resetMassData()
        ball.setGravityScale(-1.2)
        ball.setLinearDamping(0.5)
        love.physics.newDistanceJoint(tail, ball, tx, ty, tx, ty + length * 1.8, false^)
        balloon := ball
    }

    # A push to one side so it is already swinging when the window opens.
    let^first = segments[0]
    if^first fits^love.physics.Body { first.applyLinearImpulse(90, 0, true^) }

    clouds := {}
    for^i from^1 to^6 {
        clouds.push^({
            x = (i * 173) % (w + 200) - 100,
            y = 40 + ((i * 97) % 260),
            w = 60 + ((i * 53) % 70),
            speed = 12 + ((i * 31) % 22),
        })
    }
}

public^let^load = p^{
    let^renderer, version, vendor, device = love.graphics.getRendererInfo()
    love.window.setTitle($"lhatove - {renderer}")
    love.graphics.setBackgroundColor(0.17, 0.65, 0.87)
    love.physics.setMeter(64)
    build()
}

public^let^update = p^dt:number^{
    time += dt
    world.update(dt)

    let^w = love.graphics.getWidth()
    for^cloud in^clouds {
        cloud.x += cloud.speed * dt
        if^cloud.x > w + 120 { cloud.x := -120 }
    }

    # The balloon keeps its distance from the ceiling without a joint to it.
    if^balloon fits^love.physics.Body {
        let^bx, by = balloon.getPosition()
        if^by < 60 { balloon.applyForce(0, 900, true^) }
    }
}

let^cloud = p^x:number^, y:number^, w:number^{
    love.graphics.ellipse(love.graphics.DrawMode.fill, x, y, w * 0.5, w * 0.28)
    love.graphics.ellipse(love.graphics.DrawMode.fill, x - w * 0.28, y + w * 0.06, w * 0.3, w * 0.19)
    love.graphics.ellipse(love.graphics.DrawMode.fill, x + w * 0.3, y + w * 0.05, w * 0.26, w * 0.17)
}

public^let^draw = p^{
    let^w = love.graphics.getWidth()
    let^h = love.graphics.getHeight()

    love.graphics.setColor(1, 1, 1, 0.85)
    for^c in^clouds { cloud(c.x, c.y, c.w) }

    # The chain: a line through the bodies, then a letter on each.
    love.graphics.setColor(0.15, 0.28, 0.4, 1)
    love.graphics.setLineWidth(4)
    if^anchor fits^love.physics.Body {
        var^px, py = anchor.getPosition()
        for^body in^segments {
            let^x, y = body.getPosition()
            love.graphics.line(px, py, x, y)
            px := x
            py := y
        }
        if^balloon fits^love.physics.Body {
            let^bx, by = balloon.getPosition()
            love.graphics.line(px, py, bx, by)
        }
    }

    let^font = love.graphics.getFont()
    for^i from^0 to^segments.length^ - 1 {
        let^body = segments[i]
        let^letter = letters[i]
        if^body fits^love.physics.Body and^letter fits^string^{
            let^x, y = body.getPosition()
            love.graphics.setColor(1, 1, 1, 1)
            love.graphics.circle(love.graphics.DrawMode.fill, x, y, 17)
            love.graphics.setColor(0.15, 0.28, 0.4, 1)
            love.graphics.print(letter, x - font.getWidth(letter) / 2, y - font.getHeight() / 2)
        }
    }

    # The balloon, with an eye that blinks now and then.
    if^balloon fits^love.physics.Body {
        let^bx, by = balloon.getPosition()
        love.graphics.setColor(0.98, 0.85, 0.28, 1)
        love.graphics.circle(love.graphics.DrawMode.fill, bx, by, 34)
        love.graphics.setColor(0.15, 0.28, 0.4, 1)
        let^open = (time * 60) % 240 > 12
        if^open {
            love.graphics.circle(love.graphics.DrawMode.fill, bx - 11, by - 6, 4)
            love.graphics.circle(love.graphics.DrawMode.fill, bx + 11, by - 6, 4)
        else^:
            love.graphics.line(bx - 15, by - 6, bx - 7, by - 6)
            love.graphics.line(bx + 7, by - 6, bx + 15, by - 6)
        }
        love.graphics.arc(love.graphics.DrawMode.line, love.graphics.ArcMode.open, bx, by + 2, 16, std.math.rad(30), std.math.rad(150))
    }

    love.graphics.setColor(1, 1, 1, 1)
    love.graphics.printf("No game was given.\n\nDrop a game folder or a .love file on this window to run it,\nor pass one on the command line:\n\n    lovec path/to/gamedir\n    lovec path/to/packagedgame.love\n    lovec path/to/file.lh\n\nPress Escape to quit.", 40, h - 190, w - 80)
}

public^let^resize = p^width:number^, height:number^{
    build()
}

# A click lands on a body when it is inside one of its shapes; the sky
# takes no notice.
public^let^mousepressed = p^x:number^, y:number^, button:number^, istouch:bool^, presses:number^{
    for^body in^world.getBodies() {
        for^shape in^body.getShapes() {
            if^shape.testPoint(x, y) {
                let^bx, by = body.getPosition()
                var^dx = bx - x
                var^dy = by - y
                # A click dead centre still has to go somewhere: upwards.
                if^dx * dx + dy * dy < 1 { dy := -1 }
                let^push = 60 * body.getMass()
                body.applyLinearImpulse(dx * push, dy * push, true^)
            }
        }
    }
}

public^let^keypressed = p^key:love.keyboard.KeyConstant, scancode:love.keyboard.Scancode, isrepeat:bool^{
    if^key = love.keyboard.KeyConstant.escape { love.event.quit() }
}

# A game dropped on the window is what the next boot runs (nogame.lua's
# _noGameRestartInfo).
public^let^filedropped = p^path:string^{
    love.boot.restartInto(path)
}

public^let^directorydropped = p^path:string^{
    love.boot.restartInto(path)
}
)lh";


} // lh
} // love

#endif // LOVE_LH_BOOT_LH_H
