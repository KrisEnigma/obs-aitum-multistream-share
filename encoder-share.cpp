#include "encoder-share.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <string>
#include <unordered_set>

obs_data_t *find_output_settings(obs_data_array_t *outputs, const char *name)
{
	if (!outputs || !name || !name[0])
		return nullptr;

	const size_t count = obs_data_array_count(outputs);
	for (size_t i = 0; i < count; i++) {
		obs_data_t *item = obs_data_array_item(outputs, i);
		const char *item_name = obs_data_get_string(item, "name");
		if (item_name && strcmp(item_name, name) == 0)
			return item;
		obs_data_release(item);
	}
	return nullptr;
}

static obs_data_t *resolve_share_owner(obs_data_array_t *outputs, const char *share_id, const char *encoder_field,
				       const char **error_key)
{
	std::unordered_set<std::string> seen;
	std::string current = encoder_share_owner(share_id);

	while (!current.empty()) {
		if (!seen.insert(current).second) {
			blog(LOG_WARNING, "[Aitum Multistream] encoder share cycle involving '%s'", current.c_str());
			if (error_key)
				*error_key = "EncoderShareCycle";
			return nullptr;
		}

		obs_data_t *owner_settings = find_output_settings(outputs, current.c_str());
		if (!owner_settings) {
			blog(LOG_WARNING, "[Aitum Multistream] encoder share owner '%s' not found", current.c_str());
			if (error_key)
				*error_key = "EncoderShareOwnerNotFound";
			return nullptr;
		}

		const char *owner_encoder = obs_data_get_string(owner_settings, encoder_field);
		if (encoder_is_share(owner_encoder)) {
			current = encoder_share_owner(owner_encoder);
			obs_data_release(owner_settings);
			continue;
		}

		if (!owner_encoder || !owner_encoder[0]) {
			blog(LOG_WARNING,
			     "[Aitum Multistream] cannot share from '%s' — it uses Main/Vertical encoder, not a dedicated session",
			     obs_data_get_string(owner_settings, "name"));
			obs_data_release(owner_settings);
			if (error_key)
				*error_key = "EncoderShareOwnerIsMain";
			return nullptr;
		}

		return owner_settings;
	}

	if (error_key)
		*error_key = "EncoderShareInvalid";
	return nullptr;
}

static obs_encoder_t *create_or_get_video_encoder(obs_data_t *owner_settings)
{
	const char *owner_name = obs_data_get_string(owner_settings, "name");
	const char *venc_id = obs_data_get_string(owner_settings, "video_encoder");
	if (!owner_name || !owner_name[0] || !venc_id || !venc_id[0] || encoder_is_share(venc_id))
		return nullptr;

	std::string encoder_name = "aitum_multi_video_encoder_";
	encoder_name += owner_name;

	obs_encoder_t *venc = obs_get_encoder_by_name(encoder_name.c_str());
	if (venc)
		return venc;

	obs_data_t *s = nullptr;
	obs_data_t *ves = obs_data_get_obj(owner_settings, "video_encoder_settings");
	if (ves) {
		s = obs_data_create();
		obs_data_apply(s, ves);
		obs_data_release(ves);
	}

	venc = obs_video_encoder_create(venc_id, encoder_name.c_str(), s, nullptr);
	obs_data_release(s);
	if (!venc) {
		blog(LOG_ERROR, "[Aitum Multistream] failed to create video encoder '%s' (%s)", encoder_name.c_str(),
		     venc_id);
		return nullptr;
	}

	obs_encoder_set_video(venc, obs_get_video());

	const auto divisor = obs_data_get_int(owner_settings, "frame_rate_divisor");
	if (divisor > 1)
		obs_encoder_set_frame_rate_divisor(venc, (uint32_t)divisor);

	if (obs_data_get_bool(owner_settings, "scale")) {
		obs_encoder_set_scaled_size(venc, (uint32_t)obs_data_get_int(owner_settings, "width"),
					    (uint32_t)obs_data_get_int(owner_settings, "height"));
		obs_encoder_set_gpu_scale_type(venc, (obs_scale_type)obs_data_get_int(owner_settings, "scale_type"));
	}

	blog(LOG_INFO, "[Aitum Multistream] created shared video encoder '%s' (%s)", encoder_name.c_str(), venc_id);
	return venc;
}

static obs_encoder_t *create_or_get_audio_encoder(obs_data_t *owner_settings)
{
	const char *owner_name = obs_data_get_string(owner_settings, "name");
	const char *aenc_id = obs_data_get_string(owner_settings, "audio_encoder");
	if (!owner_name || !owner_name[0] || !aenc_id || !aenc_id[0] || encoder_is_share(aenc_id))
		return nullptr;

	std::string encoder_name = "aitum_multi_audio_encoder_";
	encoder_name += owner_name;

	obs_encoder_t *aenc = obs_get_encoder_by_name(encoder_name.c_str());
	if (aenc)
		return aenc;

	obs_data_t *s = nullptr;
	obs_data_t *aes = obs_data_get_obj(owner_settings, "audio_encoder_settings");
	if (aes) {
		s = obs_data_create();
		obs_data_apply(s, aes);
		obs_data_release(aes);
	}

	aenc = obs_audio_encoder_create(aenc_id, encoder_name.c_str(), s, obs_data_get_int(owner_settings, "audio_track"),
					nullptr);
	obs_data_release(s);
	if (!aenc) {
		blog(LOG_ERROR, "[Aitum Multistream] failed to create audio encoder '%s' (%s)", encoder_name.c_str(),
		     aenc_id);
		return nullptr;
	}

	obs_encoder_set_audio(aenc, obs_get_audio());
	blog(LOG_INFO, "[Aitum Multistream] created shared audio encoder '%s' (%s)", encoder_name.c_str(), aenc_id);
	return aenc;
}

obs_encoder_t *resolve_video_encoder(obs_data_t *settings, obs_data_array_t *outputs, const char **error_key)
{
	if (error_key)
		*error_key = nullptr;

	const char *venc_name = obs_data_get_string(settings, "video_encoder");
	if (!venc_name || !venc_name[0]) {
		obs_output_t *main_output = obs_frontend_get_streaming_output();
		if (!main_output || !obs_output_active(main_output)) {
			obs_output_release(main_output);
			if (error_key)
				*error_key = "MainOutputNotActive";
			return nullptr;
		}
		const int vei = (int)obs_data_get_int(settings, "video_encoder_index");
		obs_encoder_t *venc = obs_output_get_video_encoder2(main_output, vei);
		obs_output_release(main_output);
		if (!venc) {
			if (error_key)
				*error_key = "MainOutputEncoderIndexNotFound";
			return nullptr;
		}
		return obs_encoder_get_ref(venc);
	}

	if (encoder_is_share(venc_name)) {
		obs_data_t *owner_settings = resolve_share_owner(outputs, venc_name, "video_encoder", error_key);
		if (!owner_settings)
			return nullptr;
		obs_encoder_t *venc = create_or_get_video_encoder(owner_settings);
		obs_data_release(owner_settings);
		return venc;
	}

	return create_or_get_video_encoder(settings);
}

obs_encoder_t *resolve_audio_encoder(obs_data_t *settings, obs_data_array_t *outputs, const char **error_key)
{
	if (error_key)
		*error_key = nullptr;

	const char *aenc_name = obs_data_get_string(settings, "audio_encoder");
	if (!aenc_name || !aenc_name[0]) {
		obs_output_t *main_output = obs_frontend_get_streaming_output();
		if (!main_output || !obs_output_active(main_output)) {
			obs_output_release(main_output);
			if (error_key)
				*error_key = "MainOutputNotActive";
			return nullptr;
		}
		const int aei = (int)obs_data_get_int(settings, "audio_encoder_index");
		obs_encoder_t *aenc = obs_output_get_audio_encoder(main_output, aei);
		obs_output_release(main_output);
		if (!aenc) {
			if (error_key)
				*error_key = "MainOutputEncoderIndexNotFound";
			return nullptr;
		}
		return obs_encoder_get_ref(aenc);
	}

	if (encoder_is_share(aenc_name)) {
		obs_data_t *owner_settings = resolve_share_owner(outputs, aenc_name, "audio_encoder", error_key);
		if (!owner_settings)
			return nullptr;
		obs_encoder_t *aenc = create_or_get_audio_encoder(owner_settings);
		obs_data_release(owner_settings);
		return aenc;
	}

	return create_or_get_audio_encoder(settings);
}
