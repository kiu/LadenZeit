package com.nakamura_labs.ladenzeit.entity;

import java.time.Instant;
import java.util.ArrayList;
import java.util.List;

import com.fasterxml.jackson.annotation.JsonIgnore;
import com.nakamura_labs.ladenzeit.entity.common.RowCreatedAt;

import jakarta.persistence.CascadeType;
import jakarta.persistence.Column;
import jakarta.persistence.Entity;
import jakarta.persistence.Id;
import jakarta.persistence.Index;
import jakarta.persistence.JoinColumn;
import jakarta.persistence.OneToMany;
import jakarta.persistence.OrderColumn;
import jakarta.persistence.Table;

@Entity
@Table(indexes = { @Index(name = "idx_session_otp", columnList = "session_otp"),
		@Index(name = "idx_session_cookie", columnList = "session_cookie"), })
public class LZDevice extends RowCreatedAt {

	@Id
	@Column(updatable = false, nullable = false)
	private String id;

	@JsonIgnore
	private String sessionCookie;

	@JsonIgnore
	private Instant sessionCookieCreatedAt;

	@JsonIgnore
	private String sessionOtp;

	@JsonIgnore
	private Instant sessionOtpCreatedAt;

	@OneToMany(cascade = CascadeType.ALL, orphanRemoval = true)
	@JoinColumn(name = "lzdevice_id")
	@OrderColumn(name = "sort_order")
	private List<LZDevicePlace> lzDevicePlaces = new ArrayList<>();

	public String getId() {
		return this.id;
	}

	public void setId(String id) {
		this.id = id;
	}

	public String getSessionCookie() {
		return this.sessionCookie;
	}

	public void setSessionCookie(String sessionCookie) {
		this.sessionCookie = sessionCookie;
		this.sessionCookieCreatedAt = Instant.now();
	}

	public Instant getSessionCookieCreatedAt() {
		return sessionCookieCreatedAt;
	}

	public String getSessionOtp() {
		return sessionOtp;
	}

	public void setSessionOtp(String sessionOtp) {
		this.sessionOtp = sessionOtp;
		this.sessionOtpCreatedAt = Instant.now();
	}

	public Instant getSessionOtpCreatedAt() {
		return sessionOtpCreatedAt;
	}

	public void addDevicePlace(LZDevicePlace dp) {
		this.lzDevicePlaces.add(dp);
	}

	public List<LZDevicePlace> getDevicePlaces() {
		return this.lzDevicePlaces;
	}

	public LZDevice() {
		super();
	}

}