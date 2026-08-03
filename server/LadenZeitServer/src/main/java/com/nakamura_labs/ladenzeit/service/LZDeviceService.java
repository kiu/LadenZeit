package com.nakamura_labs.ladenzeit.service;

import java.time.Instant;
import java.time.temporal.ChronoUnit;
import java.util.List;
import java.util.UUID;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Service;

import com.nakamura_labs.ladenzeit.entity.LZDevice;
import com.nakamura_labs.ladenzeit.entity.LZDevicePlace;
import com.nakamura_labs.ladenzeit.misc.OtpCodeGenerator;
import com.nakamura_labs.ladenzeit.repository.LZDeviceRepository;

@Service
public class LZDeviceService {

	private static final Logger LOG = LoggerFactory.getLogger(LZDeviceService.class);

	@Autowired
	private LZDeviceRepository lzDeviceRepository;

	@Value("${com.nakamura_labs.ladenzeit.otp_expire_minutes}")
	private Long otpExpireMinutes;

	@Value("${com.nakamura_labs.ladenzeit.cookie_expire_hours}")
	private Long cookieExpireHours;

	public LZDevice getDevice(String deviceId) {
		LOG.debug("Lookup device: " + deviceId);
		LZDevice device = lzDeviceRepository.findById(deviceId).orElse(null);

		if (device == null) {
			LOG.debug("Device not found: " + deviceId);
			return null;
		}

		LOG.debug("Device found: " + deviceId);
		return device;
	}

	public String activateSessionOtp(String deviceId) {
		LOG.debug("Setting OTP: " + deviceId);

		LZDevice device = lzDeviceRepository.findById(deviceId).orElse(null);
		if (device == null) {
			LOG.warn("Device unkown: " + deviceId);
			return OtpCodeGenerator.generateRawCodeInvalid();
		}

		String otpRaw = OtpCodeGenerator.generateRawCode();

		device.setSessionOtp(otpRaw);
		device.setSessionCookie(UUID.randomUUID().toString());
		lzDeviceRepository.save(device);
		return OtpCodeGenerator.formatWithHyphens(otpRaw);
	}

	public String sessionCreateByOtp(String otp) {
		LOG.debug("Lookup OTP: " + otp);

		LZDevice device = lzDeviceRepository.findBySessionOtp(otp);
		if (device == null) {
			LOG.debug("OTP not found: " + otp);
			return null;
		}

		if (ChronoUnit.MINUTES.between(device.getSessionOtpCreatedAt(), Instant.now()) > this.otpExpireMinutes) {
			LOG.debug("Device known, but otp expired: " + device.getId());
			return null;
		}

		device.setSessionOtp(null);
		lzDeviceRepository.save(device);

		return device.getSessionCookie();
	}

	public LZDevice sessionVerify(String sessionCookie) {
		if (sessionCookie == null || sessionCookie.isBlank()) {
			LOG.error("Cookie is null, this shouldn't be possible");
			return null;
		}

		LZDevice device = lzDeviceRepository.findBySessionCookie(sessionCookie);
		if (device == null) {
			LOG.debug("Session Id unknown.");
			return null;
		}
		
		if (ChronoUnit.HOURS.between(device.getSessionCookieCreatedAt(), Instant.now()) > this.cookieExpireHours) {
			LOG.debug("Device known, but cookie expired: " + device.getId());
			return null;
		}

		return device;
	}

	public void sessionInvalidate(LZDevice device) {
		if (device == null) {
			LOG.error("Device is null, this shouldn't be possible");
			return;
		}

		device.setSessionCookie(null);
		lzDeviceRepository.save(device);
	}

	public void replacePlaces(LZDevice device, List<LZDevicePlace> newPlaces) {
		if (device == null) {
			LOG.error("Device is null, this shouldn't be possible");
			return;
		}

		device.getDevicePlaces().clear();
		device.getDevicePlaces().addAll(newPlaces);
		lzDeviceRepository.save(device);
	}

}
