package com.nakamura_labs.ladenzeit.entity;

import com.fasterxml.jackson.annotation.JsonIgnore;
import com.nakamura_labs.ladenzeit.entity.common.RowCreatedAt;

import jakarta.persistence.Column;
import jakarta.persistence.Entity;
import jakarta.persistence.GeneratedValue;
import jakarta.persistence.GenerationType;
import jakarta.persistence.Id;

@Entity
public class LZPlaceHour extends RowCreatedAt {

	@Id
	@GeneratedValue(strategy = GenerationType.UUID)
	@Column(updatable = false, nullable = false)
	@JsonIgnore
	private String id;

	
	@Column(updatable = false, nullable = false)
	private String fromString;

	@Column(updatable = false, nullable = false)
	private Integer fromWeekday;

	@Column(updatable = false, nullable = false)
	private Integer fromInteger;

	
	@Column(updatable = false, nullable = false)
	private String toString;
	
	@Column(updatable = false, nullable = false)
	private Integer toWeekday;

	@Column(updatable = false, nullable = false)
	private Integer toInteger;

	public String getFromString() {
		return fromString;
	}

	public void setFromString(String fromString) {
		this.fromString = fromString;
	}

	public Integer getFromWeekday() {
		return fromWeekday;
	}

	public void setFromWeekday(Integer fromWeekday) {
		this.fromWeekday = fromWeekday;
	}

	public Integer getFromInteger() {
		return fromInteger;
	}

	public void setFromInteger(Integer fromInteger) {
		this.fromInteger = fromInteger;
	}


	public String getToString() {
		return toString;
	}

	public void setToString(String toString) {
		this.toString = toString;
	}

	public Integer getToWeekday() {
		return toWeekday;
	}

	public void setToWeekday(Integer toWeekday) {
		this.toWeekday = toWeekday;
	}
	
	public void setToInteger(Integer toInteger) {
		this.toInteger = toInteger;
	}

	public Integer getToInteger() {
		return toInteger;
	}

	public LZPlaceHour() {
		super();
	}

}