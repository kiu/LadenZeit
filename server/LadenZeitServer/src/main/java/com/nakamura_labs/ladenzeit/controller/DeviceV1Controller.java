package com.nakamura_labs.ladenzeit.controller;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.time.ZoneId;
import java.time.ZonedDateTime;
import java.util.ArrayList;
import java.util.List;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.core.io.ByteArrayResource;
import org.springframework.http.HttpStatus;
import org.springframework.http.MediaType;
import org.springframework.http.ResponseEntity;
import org.springframework.validation.annotation.Validated;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PatchMapping;
import org.springframework.web.bind.annotation.PathVariable;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

import com.nakamura_labs.ladenzeit.entity.LZDevice;
import com.nakamura_labs.ladenzeit.entity.LZDevicePlace;
import com.nakamura_labs.ladenzeit.entity.LZPlace;
import com.nakamura_labs.ladenzeit.entity.LZPlaceHour;
import com.nakamura_labs.ladenzeit.service.LZDeviceService;
import com.nakamura_labs.ladenzeit.service.LZPlaceService;

@RestController
@RequestMapping("/api/v1/devices")
@Validated
public class DeviceV1Controller {

	private static final Logger LOG = LoggerFactory.getLogger(DeviceV1Controller.class);

	@Autowired
	private LZDeviceService lzDeviceService;

	@Autowired
	private LZPlaceService lzPlaceService;

	@GetMapping("/{deviceId}/places")
	public ResponseEntity<Object> places(@PathVariable String deviceId) {
		LOG.debug("Places request for device: " + deviceId);

		LZDevice device = lzDeviceService.getDevice(deviceId);

		List<LZDevicePlace> dps = new ArrayList<LZDevicePlace>();
		if (device != null) {
			dps = device.getDevicePlaces();
		}

		ByteArrayOutputStream baosPayload = new ByteArrayOutputStream();
		try {
			LOG.debug("Count: " + dps.size());
			baosPayload.write(dps.size());
			
			for (LZDevicePlace dp : dps) {
				LOG.debug("Place: " + dp.getPlaceName());
				baosPayload.write(dp.getPlaceName().length() + 1);

				LOG.debug("Len: " + (dp.getPlaceName().length() + 1));
				baosPayload.write(dp.getPlaceName().getBytes());
				baosPayload.write(0);

				LZPlace p = lzPlaceService.getPlace(dp.getPlaceId());
				p.sortPlaceHours();
				List<LZPlaceHour> phs = p.getPlaceHours();
				LOG.debug("Slots: " + phs.size());
				baosPayload.write(phs.size());

				for (LZPlaceHour ph : phs) {
					LOG.debug("Open: " + ph.getFromWeekday() + " " + ph.getFromString() + " (" + ph.getFromInteger()
							+ ")");
					baosPayload.write((byte) ((ph.getFromInteger() >> 8) & 0xFF));
					baosPayload.write((byte) ((ph.getFromInteger() >> 0) & 0xFF));

					LOG.debug("Close: " + ph.getToWeekday() + " " + ph.getToString() + " (" + ph.getToInteger() + ")");
					baosPayload.write((byte) ((ph.getToInteger() >> 8) & 0xFF));
					baosPayload.write((byte) ((ph.getToInteger() >> 0) & 0xFF));
				}
			}
		} catch (IOException e) {
			LOG.error("Exception while generating places list.", e);
			return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR).build();
		}

		ByteArrayOutputStream baosHeader = new ByteArrayOutputStream();
		try {
			ZonedDateTime now = ZonedDateTime.now(ZoneId.of("Europe/Berlin"));
			long epoch = now.toEpochSecond() + now.getOffset().getTotalSeconds();
			int b32 = (int) epoch;

			LOG.debug("Epoch (adjusted to Timezone): " + epoch);
			baosHeader.write((byte) ((b32 >> 24) & 0xFF));
			baosHeader.write((byte) ((b32 >> 16) & 0xFF));
			baosHeader.write((byte) ((b32 >> 8) & 0xFF));
			baosHeader.write((byte) (b32 & 0xFF));
			
			baosPayload.writeTo(baosHeader);
		} catch (IOException e) {
			LOG.error("Exception while generating header.", e);
			return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR).build();
		}

		byte[] data = baosHeader.toByteArray();
		return ResponseEntity.ok().contentLength(data.length)
//				.header(HttpHeaders.CONTENT_DISPOSITION, "attachment; filename=\"data.bin\"")
				.contentType(MediaType.APPLICATION_OCTET_STREAM).body(new ByteArrayResource(data));
	}

	@PatchMapping("/{deviceId}/auth/otp")
	public ResponseEntity<Object> authOtp(@PathVariable String deviceId) {
		LOG.debug("Web-OTP request for device: " + deviceId);
		String otpFormatted = lzDeviceService.activateSessionOtp(deviceId);
		return ResponseEntity.status(HttpStatus.OK).body(otpFormatted);
	}

}