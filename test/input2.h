#ifndef MEDIMGRENDERALGO_RAY_CASTER_H
#define MEDIMGRENDERALGO_RAY_CASTER_H

#include "renderalgo/mi_render_algo_export.h"

#include "arithmetic/mi_matrix4.h"
#include "arithmetic/mi_vector2f.h"
#include "arithmetic/mi_vector4f.h"

#include "renderalgo/mi_render_algo_define.h"
#include "renderalgo/mi_gpu_resource_pair.h"
#include "mi_brick_define.h"

MED_IMG_BEGIN_NAMESPACE

class EntryExitPoints;
class ImageData;
class CameraBase;
class CameraCalculator;
class RayCasterInnerResource;
class Canvas;
class RayCastingCPU;
class RayCastingGPUGL;
class RayCastingGPUCUDA;
class RayCastingCPUBrickAcc;

class RayCaster
        : public std::enable_shared_from_this<RayCaster> {
    friend class RayCastingCPU;
    friend class RayCastingCPUBrickAcc;
    friend class RayCastingGPUGL;
    friend class RayCastingGPUCUDA;

public:
    RayCaster(ProcessingUnit process_unit, GPUPlatform gpu_platform);

    ~RayCaster();

    void render();

    void on_entry_exit_points_resize(int width, int height);

    void set_test_code(int test_code);
    int get_test_code() const;

    void set_canvas(std::shared_ptr<Canvas> canvas);
    std::shared_ptr<Canvas> get_canvas();

    // Mask label level
    // Default is L_8
    void set_mask_label_level(LabelLevel label_level);
    LabelLevel get_mask_label_level() const;

    //////////////////////////////////////////////////////////////////////////
    // Input data
    //////////////////////////////////////////////////////////////////////////

    // Volume & mask texture/array
    void set_volume_data(std::shared_ptr<ImageData> image_data);
    std::shared_ptr<ImageData> get_volume_data();

    void set_mask_data(std::shared_ptr<ImageData> image_data);
    std::shared_ptr<ImageData> get_mask_data();

    void set_volume_data_texture(GPUTexture3DPairPtr volume_textures);
    GPUTexture3DPairPtr get_volume_data_texture();

    void set_mask_data_texture(GPUTexture3DPairPtr mask_textures);
    GPUTexture3DPairPtr get_mask_data_texture();

    // Entry exit points
    void set_entry_exit_points(std::shared_ptr<EntryExitPoints> entry_exit_points);

    std::shared_ptr<EntryExitPoints> get_entry_exit_points() const;

    //////////////////////////////////////////////////////////////////////////
    // Ray casting parameter
    //////////////////////////////////////////////////////////////////////////

    // Volume modeling parameter
    void set_camera(std::shared_ptr<CameraBase> camera);
    std::shared_ptr<CameraBase> get_camera() const;

    void set_camera_calculator(std::shared_ptr<CameraCalculator> camera_cal);
    std::shared_ptr<CameraCalculator> get_camera_calculator() const;

    // Sample rate
    void set_sample_step(float sample_step);
    float get_sample_step() const;

    // Label parameter
    void set_visible_labels(std::vector<unsigned char> labels);
    const std::vector<unsigned char>& get_visible_labels() const;

    //--------------------------------------//
    // Window level parameter
    // Note: here WL is regulated
    void set_window_level(float ww, float wl, unsigned char label);
    void get_window_levels(std::map<unsigned char, Vector2f>& wls) const;
    void get_visible_window_levels(std::map<unsigned char, Vector2f>& wls) const;
    void set_global_window_level(float ww, float wl);

    void get_global_window_level(float& ww, float& wl) const;

    void set_minip_threashold(float th);
    float get_minip_threashold() const;

    // Pseudo color parameter
    void set_pseudo_color_texture(GPUTexture1DPairPtr tex, unsigned int length);
    GPUTexture1DPairPtr get_pseudo_color_texture(unsigned int& length) const;
    void set_pseudo_color_array(unsigned char* color_array, unsigned int length);

    void set_color_opacity_texture_array(GPUTexture1DArrayPairPtr tex_array);
    GPUTexture1DArrayPairPtr get_color_opacity_texture_array() const;

    // Mask overlay color
    void set_mask_overlay_color(std::map<unsigned char, RGBAUnit> colors);
    void set_mask_overlay_color(RGBAUnit color, unsigned char label);
    const std::map<unsigned char, RGBAUnit>& get_mask_overlay_color() const;
    void set_mask_overlay_opacity(float opacity);
    float get_mask_overlay_opacity() const;

    // Enhancement parameter
    void set_sillhouette_enhancement();
    void set_boundary_enhancement();

    // Shading parameter
    void set_ambient_color(float r, float g, float b, float factor);
    void get_ambient_color(float(&rgba)[4]);
    void set_material(const Material& m, unsigned char label);

    // Jittering to prevent wooden artifacts
    void set_jittering(bool flag);
    bool get_jittering() const;

    void set_random_texture(GPUTexture2DPairPtr tex);
    GPUTexture2DPairPtr get_random_texture() const;

    // Adjust ray start align to view plane.
    // VR set true; MPR set false
    void set_ray_align_to_view_plane(bool flag);
    bool get_ray_align_to_view_plane() const;

    // Bounding box //TODO
    void set_bounding(const Vector3f& min, const Vector3f& max);

    // Clipping plane
    void set_clipping_plane_function(const std::vector<Vector4f>& funcs);

    // Ray casting mode parameter
    void set_mask_mode(MaskMode mode);
    void set_composite_mode(CompositeMode mode);
    void set_interpolation_mode(InterpolationMode mode);
    void set_shading_mode(ShadingMode mode);
    void set_color_inverse_mode(ColorInverseMode mode);
    void set_mask_overlay_mode(MaskOverlayMode mode);

    MaskMode get_mask_mode() const;
    CompositeMode get_composite_mode() const;
    InterpolationMode get_interpolation_mode() const;
    ShadingMode get_shading_mode() const;
    ColorInverseMode get_color_inverse_mode() const;
    MaskOverlayMode get_mask_overlay_mode() const;

    // Inner buffer
    std::shared_ptr<RayCasterInnerResource> get_inner_resource();

    void set_downsample(bool flag);
    bool get_downsample() const;

    void set_ds_canvas_percent(int percent);
    int get_ds_canvas_percent() const;

    void set_ds_dvr_sample_step(float ds_dvr_sample_step);
    float get_ds_dvr_sample_step() const;

    //just work for CUDA VR
    void set_cuda_gl_interop(bool flag);
    bool get_cuda_gl_interop() const;

private:
    void render_cpu();
    void render_gpu_gl();
    void render_gpu_cuda();
private:
    // Processing unit type
    ProcessingUnit _process_unit;
    //GPU platform
    GPUPlatform _gpu_platform;

    //Mask label
    LabelLevel _mask_label_level;

    // Input data
    std::shared_ptr<ImageData> _volume_data;
    GPUTexture3DPairPtr _volume_textures;

    std::shared_ptr<ImageData> _mask_data;
    GPUTexture3DPairPtr _mask_textures;

    // Entry exit points
    std::shared_ptr<EntryExitPoints> _entry_exit_points;

    std::shared_ptr<CameraBase> _camera;
    std::shared_ptr<CameraCalculator> _camera_cal;

    // Data sample step(DVR 0.5 voxel , MIPs 1.0 voxel)
    float _sample_step;
    float _custom_sample_step;

    // Global window level for MIPs mode
    float _global_ww;
    float _global_wl;

    // MipIP threshold
    float _minip_threshold;

    // Transfer function & pseudo color
    GPUTexture1DArrayPairPtr _color_opacity_texture_array;
    GPUTexture1DPairPtr _pseudo_color_texture;
    unsigned char* _pseudo_color_array;
    unsigned int _pseudo_color_length;

    // Inner buffer to contain label based parameter
    std::shared_ptr<RayCasterInnerResource> _inner_resource;

    // DVR jittering flag
    bool _jittering;
    // random texture for jittering
    GPUTexture2DPairPtr _random_texture;

    // VR ray cas
    bool _ray_align_to_view_plane;

    // Bounding
    Vector3f _bound_min;
    Vector3f _bound_max;

    // Clipping plane
    std::vector<Vector4f> _clipping_planes;

    // Ray casting mode
    MaskMode _mask_mode;
    CompositeMode _composite_mode;
    InterpolationMode _interpolation_mode;
    ShadingMode _shading_mode;
    ColorInverseMode _color_inverse_mode;
    MaskOverlayMode _mask_overlay_mode;

    // Mask overlay opacity
    float _mask_overlay_opacity;

    // Ambient
    float _ambient_color[4];

    std::shared_ptr<RayCastingCPU> _ray_casting_cpu;
    std::shared_ptr<RayCastingGPUGL> _ray_casting_gpu_gl;
    std::shared_ptr<RayCastingGPUCUDA> _ray_casting_gpu_cuda;

    // Canvas for rendering
    std::shared_ptr<Canvas> _canvas;

    float _pre_rendering_duration;

    bool _downsample;

    int _ds_canvas_percent;
    float _ds_dvr_sample_step;

    //CUDA GL interop flag for CUDA VR
    bool _cuda_gl_interop;

    // Test code for debug
    int _test_code;
};

MED_IMG_END_NAMESPACE

#endif