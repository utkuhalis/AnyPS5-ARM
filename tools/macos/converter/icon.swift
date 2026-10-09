// Draws the AnyPS5 app icon: swift icon.swift <output.png>
import AppKit

let canvas: CGFloat = 1024
let inset: CGFloat = 100
let image = NSImage(size: NSSize(width: canvas, height: canvas))
image.lockFocus()
let tile = NSRect(x: inset, y: inset, width: canvas - 2 * inset, height: canvas - 2 * inset)
let shape = NSBezierPath(roundedRect: tile, xRadius: 185, yRadius: 185)
NSGraphicsContext.current?.saveGraphicsState()
let shadow = NSShadow()
shadow.shadowColor = NSColor(calibratedRed: 0.2, green: 0.1, blue: 0.5, alpha: 0.45)
shadow.shadowBlurRadius = 28
shadow.shadowOffset = NSSize(width: 0, height: -12)
shadow.set()
NSColor.black.setFill()
shape.fill()
NSGraphicsContext.current?.restoreGraphicsState()
NSGradient(colors: [NSColor(calibratedRed: 0.28, green: 0.36, blue: 0.98, alpha: 1), NSColor(calibratedRed: 0.55, green: 0.27, blue: 0.93, alpha: 1)])!.draw(in: shape, angle: -45)
let configuration = NSImage.SymbolConfiguration(pointSize: 430, weight: .semibold)
if let symbol = NSImage(systemSymbolName: "gamecontroller.fill", accessibilityDescription: nil)?.withSymbolConfiguration(configuration) {
    let tinted = NSImage(size: symbol.size)
    tinted.lockFocus()
    symbol.draw(at: .zero, from: .zero, operation: .sourceOver, fraction: 1)
    NSColor.white.set()
    NSRect(origin: .zero, size: symbol.size).fill(using: .sourceAtop)
    tinted.unlockFocus()
    let origin = NSPoint(x: (canvas - symbol.size.width) / 2, y: (canvas - symbol.size.height) / 2)
    tinted.draw(at: origin, from: .zero, operation: .sourceOver, fraction: 1)
}
image.unlockFocus()
let bitmap = NSBitmapImageRep(data: image.tiffRepresentation!)!
try! bitmap.representation(using: .png, properties: [:])!.write(to: URL(fileURLWithPath: CommandLine.arguments[1]))
