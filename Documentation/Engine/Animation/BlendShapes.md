# Blend Shapes

Blend shapes are built into RendererCore, GameEngine and the standard Assets editor module. No additional plugin is required.

## Import and pose

Import an Animated Mesh with **Import blend shapes** enabled. The standard importer creates the skeleton, animated mesh and animation clips, plus a companion `.ezBlendShapeAsset`. The mesh references it through **DefaultBlendShapes**. Existing meshes can use **Import Blend Shapes** in their toolbar.

Opening the companion displays its associated mesh and imported target names/default weights. Add **Blend Shape Pose Component** beside an ordinary Animated Mesh Component to expose the names and edit weights directly in the scene. Manual overrides take priority over animated values; removing an override releases that shape back to animation.

## Animation clips

Opening a normal Animation Clip automatically fills **Blend Shapes** from its animation source and **PreviewMesh**, including shapes with no source animation channel. Changing the source clip, frame range or preview mesh refreshes the list. Names come from imported geometry; there is no manual Add Entry workflow.

The Blend Shapes timeline edits the existing named tracks. The model preview updates while dragging, including undo/redo, without saving or transforming the asset. Both Curves and Blend Shapes show a color-coded list of names beside the graph. Selecting a name selects its control points; selecting points highlights their curve names. Editing keys enables **OverrideSource** for that track. Other tracks continue following the source. Undo/redo preserves edits. Turn OverrideSource off and transform to restore the imported curve/default. ImportBlendShapes controls source morph animation import; the selected mesh still provides names available for authoring.

Animation Graphs preserve the curves through sampling, blend spaces, Lerp Poses, Switch Pose and Sample Frame. **Blend Shape: {Shape}** sets one named weight on a local pose before Pose Result.

## AngelScript

The component exposes the standard reflected scripting API:

```angelscript
ezBlendShapePoseComponent@ pose;
if (GetOwner().TryGetComponentOfBaseType(@pose))
{
    pose.SetWeight("Face/Smile", 0.75f);
    pose.RemoveWeight("Face/Smile"); // Return control to the animation.
}
```

GetWeight reads the effective value. ResetWeights clears all manual overrides.

## Performance and limits

Weights with absolute value at or below WeightThreshold (default 0.001) become zero. Unchanged weights skip deformation and GPU upload. A neutral instance uses the shared original mesh without allocating a morph buffer. Nonzero changed poses accumulate sparse deltas on the CPU and upload position/normal/tangent streams, followed by ordinary GPU skinning. There is no per-target GPU loop; the cost is CPU work and vertex upload for changing poses.

Weights are finite and limited to [-1, 1]. Names use nodeName/targetName. Morph animation import currently supports GLB/glTF. STEP interpolation uses a short transition; editable times use 1/4800-second ticks. Automatic mesh simplification is unsupported because it breaks vertex correspondence. Use authored LODs and skinned source geometry.

## Verification

Build Editor, BlendShapeTest and BlendShapeEditorTest in the same configuration. Run both tests with `-noGui -assert 0 -renderer DX11`. The editor regression checks native-only import, deferred Qt events/layout restoration in the companion window, automatic tracks before Transform, names from a separate preview model, override preservation, scene posing and curve preview changes.

Projects created with the earlier plugin prototype must remove the obsolete BlendShape/ezBlendShapePlugin entries from Editor/PluginSelection.ddl and RuntimeConfigs/Plugins.ddl. The resource, component and asset type names remain compatible. Remove the old BlendShape plugin bundle and DLLs from the output directory after rebuilding.
