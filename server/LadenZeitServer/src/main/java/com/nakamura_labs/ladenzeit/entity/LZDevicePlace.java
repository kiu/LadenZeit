package com.nakamura_labs.ladenzeit.entity;

import com.fasterxml.jackson.annotation.JsonIgnore;
import com.nakamura_labs.ladenzeit.entity.common.RowCreatedAt;

import jakarta.persistence.Column;
import jakarta.persistence.Entity;
import jakarta.persistence.GeneratedValue;
import jakarta.persistence.GenerationType;
import jakarta.persistence.Id;
import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.Pattern;
import jakarta.validation.constraints.Size;

@Entity
public class LZDevicePlace extends RowCreatedAt {

	@Id
	@GeneratedValue(strategy = GenerationType.UUID)
	@Column(updatable = false, nullable = false)
	@JsonIgnore
	private String id;

	@Column(updatable = false, nullable = false)
	@NotBlank(message = "Place Id cannot be empty")
	private String placeId;

	@Column(updatable = false, nullable = false)
	@NotBlank(message = "Place name cannot be empty")
	@Size(max = 8, message = "Place name cannot exceed 8 characters")
	@Pattern(regexp = "^[A-Za-z0-9 ^!\"$%&/{}()\\[\\]=?\\`'~+\\-*<|>;,:._#@\\\\]{1,8}$", message = "Place name has invalid characters")

	private String placeName;

	public String getPlaceId() {
		return placeId;
	}

	public String getPlaceName() {
		return placeName;
	}

	public LZDevicePlace() {
		super();
	}
}