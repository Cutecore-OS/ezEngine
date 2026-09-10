# Grey Boxing Tools

A separate editor plugin with an optional procedural cone runtime component. No engine source changes. Scenes containing cones or shading overrides require ezGreyBoxPlugin at runtime. Existing Grey Boxing components remain native until an override is requested.

## Build and enable

Build the `EditorPluginGreyBox` CMake target (it also builds `GreyBoxPlugin`). In the editor's plugin selection, enable **Grey Boxing Tools** and restart the editor if requested.

## Usage

The existing Grey Boxing component has **Recalculate Position**, an anchor dropdown and a cursor toggle in the collapsible **Tools** group, below **Misc**.

- Choose Center, a corner, a face center or an edge center (27 bounding-box anchors).
- The cursor toggle displays clickable squares in the viewport: yellow corners, purple center, smaller RGB face centers, and RGB edge centers along their corresponding axis.
- Clicking a square selects its dropdown entry. It does not change the scene.
- Click **Recalculate Position** to move the object origin and compensate the Size fields. Geometry and children stay in place.
- Selecting another object or activating a different manipulator exits picking. The previous transform tool is restored when picking ends.
- Recalculation supports multiple selected components in one Undo/Redo transaction. Viewport picking requires a single component.

For a local X interval [-36, -34], choosing Min corner produces SizeNegX = 0 and SizePosX = 2. Center produces 1 and 1 (the interval [-1, 1]). ezEngine stores the negative-side distance in SizeNegX, rather than the signed minimum coordinate.

All Grey Boxing components on the same object are compensated together. If several are selected on one object, the first selected component supplies the anchor. Non-Grey-Box components on that object must be moved to a child first, because their individual spatial data cannot be compensated generically. A parent with zero scale is rejected. Any failure rolls back the entire operation.

## Vertex snapping in Grey-Boxing (B)

Activate the existing Grey-Boxing tool with **B**. Hover over a Grey Boxing mesh to see small yellow vertex handles. Only the hovered mesh and the pinned source point are shown.

1. Click a vertex to pin the source point. It becomes larger and orange, and remains visible when the pointer leaves the mesh.
2. Hover another Grey Boxing mesh and click a target vertex. The source game object moves so that the two points coincide. Rotation, scale, Size fields and mesh shape are unchanged; child objects move together with their parent.
3. The source point is cleared after a successful snap. The next click starts a new pair.

**Ctrl** or **Shift** temporarily hides the handles and passes input through to the original box-creation tool. The pinned source is retained. **Escape** cancels the pinned point; switching away from B also clears it. Clicking another vertex on the same object changes the source point.

These are full-detail geometry vertices from the engine's own Grey Boxing generator, including every stair tread and the actual column/arch subdivisions. Coincident vertices split by UV seams or normals share one handle. Geometry is cached until its shape properties change, while handles follow current object transforms.

Snapping is one Undo/Redo transaction. A descendant of the source object cannot be a stationary target, since moving its parent would move the target too. A source parent with zero scale is rejected. If the source geometry changes and its vertex disappears, the source selection is cleared.

## Cone shape

Choose **Cone** in the existing **Shape** dropdown, or add a **Grey Boxing Extended** from Construction.

- **Sides** (3-128): polygon count around the base. Use 4 for a square pyramid, 32 or more for a round cone.
- **BaseRadiusScale** (0-4): base radius relative to half the Size X/Y dimensions. 1 fills the usual base; 0 produces an inverted apex when the top is nonzero.
- **TopRadiusScale** (0-4): 0 makes a pointed cone, a positive value makes a capped frustum. Equal base and top radii make a column.
- **HeightSegments** (1-64): subdivisions along local Z. Increase this to resolve the curved profile.
- **ProfileCurve** (-0.95 to 1): 0 is straight; negative values pull the profile inward and positive values bulge it outward. Both end radii stay fixed. This changes the whole side profile, rather than beveling individual edges.
- **SmoothShading**: smooth side normals. Disable for a faceted pyramid or low-poly cone; cap normals always remain flat.

The cone points along local +Z. Size fields set its height, elliptical X/Y scale and local offset. Four sides form an axis-aligned square. Radius scales above 1 and an outward curve can extend beyond the nominal Size box.

Choosing Cone on a native component upgrades it in one Undo transaction, preserving its GUID, component order, shared properties and owner transform. The extended component retains the parameters for both cone and native shapes when switching Shape. Undo restores the original native component. Both forms support Tools / Recalculate Position and vertex snapping with B. Cone handles use the actual generated vertices at every height segment.

The bundle loads `ezGreyBoxPlugin` in the engine process and exports it with scenes. Rendering, static collision, world geometry extraction and occlusion use the existing engine APIs. Restart the editor after updating the plugin binaries/bundle.

## Smooth Shading

**Smooth Shading** always appears in **Misc**, both on native Grey Boxing and on Grey Boxing Extended. It uses the same control and placement before and after changing the value: label in the standard left column, checkbox in the standard value column.

The initial checkbox state is determined from the native generator's actual vertex normals for the current shape and parameters. A box and straight ramps start unchecked; a column starts checked. Curved arches and stairs can contain smooth surfaces. Changing native shape parameters refreshes the checkbox; mixed selections show a partial check.

Enabling preserves authored smooth normals where present. On a wholly faceted shape such as a box, it averages normals at shared positions using corner-angle weights. Acute creases do not blend back-facing surfaces into each other, avoiding unstable normals at ramp toes. Disabling uses flat polygon normals. Neither mode changes positions, UVs or the silhouette.

Changing the checkbox upgrades a native component in one Undo transaction. Its GUID, owner, sizes and native shape parameters are preserved. The control remains in Misc. Extended components serialize the chosen state. Selecting a different Shape updates both Shape and Smooth Shading in one document transaction. Runtime setters preserve the serialized shading value; they never silently reset another reflected property. Undo/Redo restores both values together.

Version 1 cone scenes remain readable. The serialized class name remains ezGreyBoxConeComponent for compatibility, while the editor label is Grey Boxing Extended.

## Verification

`EditorPluginGreyBoxTest` covers all anchors, rotated objects, non-uniform and negative scales, transformed parents, children, sibling grey boxes, Undo/Redo, rollback, handle picking, the collapsible Tools group, full geometry for all 13 shapes, each tread of a staircase, curvature rounding, vertex-to-vertex translation and modifier-key pass-through.


Cone tests also cover apex triangles, caps, winding, pyramids, frusta, inverted cones, profile curves, normals, settings/world serialization, CPU mesh export, collision flags, component conversion via the Shape widget, preserved world vertices and Undo/Redo.


Native shading tests compare positions and UVs for all 13 shapes, check flat face normals and changed column normals, verify preserved parameters, atomic rollback, the actual UI toggle, world serialization and version 1 cone compatibility.


Regression checks also verify property-grid columns, ramp normals and finite orthogonal tangent frames for four orientations and multiple slopes, and Shape/dropdown/checkbox/runtime consistency through Undo/Redo.
